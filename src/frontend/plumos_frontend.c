#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct log_ctx {
  FILE *log;
  int stdout_enabled;
};

static void emit(struct log_ctx *ctx, const char *fmt, ...) {
  va_list ap;
  va_list aq;

  va_start(ap, fmt);
  va_copy(aq, ap);
  if (ctx->stdout_enabled) {
    vprintf(fmt, ap);
    printf("\n");
  }
  if (ctx->log) {
    vfprintf(ctx->log, fmt, aq);
    fprintf(ctx->log, "\n");
    fflush(ctx->log);
  }
  va_end(aq);
  va_end(ap);
}

static int is_dir(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_file(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int join_path(char *out, size_t out_size, const char *a, const char *b) {
  size_t a_len = strlen(a);
  size_t b_len = strlen(b);

  if (a_len + 1 + b_len + 1 > out_size) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }

  memcpy(out, a, a_len);
  out[a_len] = '/';
  memcpy(out + a_len + 1, b, b_len);
  out[a_len + 1 + b_len] = '\0';
  return 1;
}

static char *read_file(const char *path, size_t *size_out) {
  FILE *f = fopen(path, "rb");
  char *buf = NULL;
  long size = 0;

  if (!f) {
    return NULL;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }

  size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);

  buf = (char *)calloc((size_t)size + 1, 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }

  if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
    free(buf);
    fclose(f);
    return NULL;
  }

  fclose(f);
  if (size_out) {
    *size_out = (size_t)size;
  }
  return buf;
}

static const char *skip_ws(const char *p) {
  while (*p && isspace((unsigned char)*p)) {
    p++;
  }
  return p;
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_size) {
  char pattern[128];
  const char *p = json;

  if (!json || !key || out_size == 0) {
    return 0;
  }

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  out[0] = '\0';

  while ((p = strstr(p, pattern)) != NULL) {
    const char *q = p + strlen(pattern);
    size_t n = 0;

    q = skip_ws(q);
    if (*q != ':') {
      p++;
      continue;
    }
    q++;
    q = skip_ws(q);
    if (*q != '"') {
      p++;
      continue;
    }
    q++;

    while (*q && *q != '"' && n + 1 < out_size) {
      if (*q == '\\' && q[1]) {
        q++;
      }
      out[n++] = *q++;
    }
    out[n] = '\0';
    return 1;
  }

  return 0;
}

static int cmp_strptr(const void *a, const void *b) {
  const char *aa = *(const char *const *)a;
  const char *bb = *(const char *const *)b;
  return strcmp(aa, bb);
}

static char **list_dirs(const char *path, size_t *count_out) {
  DIR *dir = opendir(path);
  struct dirent *ent;
  char **items = NULL;
  size_t count = 0;
  size_t cap = 0;

  *count_out = 0;
  if (!dir) {
    return NULL;
  }

  while ((ent = readdir(dir)) != NULL) {
    char child[PATH_MAX];
    char *name_copy;

    if (ent->d_name[0] == '.') {
      continue;
    }

    join_path(child, sizeof(child), path, ent->d_name);
    if (!is_dir(child)) {
      continue;
    }

    if (count == cap) {
      size_t next_cap = cap ? cap * 2 : 32;
      char **next = (char **)realloc(items, next_cap * sizeof(char *));
      if (!next) {
        break;
      }
      items = next;
      cap = next_cap;
    }

    name_copy = strdup(ent->d_name);
    if (!name_copy) {
      break;
    }
    items[count++] = name_copy;
  }

  closedir(dir);
  qsort(items, count, sizeof(char *), cmp_strptr);
  *count_out = count;
  return items;
}

static void free_list(char **items, size_t count) {
  size_t i;
  for (i = 0; i < count; i++) {
    free(items[i]);
  }
  free(items);
}

static int scan_category(struct log_ctx *ctx, const char *sd_root, const char *category,
                         const char *display_key) {
  char base[PATH_MAX];
  char **dirs;
  size_t count = 0;
  size_t i;
  int configs = 0;

  join_path(base, sizeof(base), sd_root, category);
  dirs = list_dirs(base, &count);
  emit(ctx, "[%s] path=%s dirs=%zu", category, base, count);

  for (i = 0; i < count; i++) {
    char dir_path[PATH_MAX];
    char config_path[PATH_MAX];
    char label[256] = "";
    char rompath[512] = "";
    char extlist[512] = "";
    char launch[256] = "";
    char *json;

    join_path(dir_path, sizeof(dir_path), base, dirs[i]);
    join_path(config_path, sizeof(config_path), dir_path, "config.json");
    if (!is_file(config_path)) {
      emit(ctx, "  - %s config=missing", dirs[i]);
      continue;
    }

    json = read_file(config_path, NULL);
    if (!json) {
      emit(ctx, "  - %s config=unreadable", dirs[i]);
      continue;
    }

    json_get_string(json, display_key, label, sizeof(label));
    if (!label[0] && strcmp(display_key, "label") != 0) {
      json_get_string(json, "label", label, sizeof(label));
    }
    json_get_string(json, "rompath", rompath, sizeof(rompath));
    json_get_string(json, "extlist", extlist, sizeof(extlist));
    json_get_string(json, "launch", launch, sizeof(launch));

    emit(ctx, "  - %s label=\"%s\" launch=\"%s\" rompath=\"%s\" extlist=\"%s\"",
         dirs[i], label[0] ? label : "(none)", launch[0] ? launch : "(none)",
         rompath[0] ? rompath : "(none)", extlist[0] ? extlist : "(none)");
    configs++;
    free(json);
  }

  free_list(dirs, count);
  return configs;
}

static void ensure_log_dir(const char *root) {
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/logs", root);
  mkdir(root, 0755);
  mkdir(path, 0755);
}

int main(int argc, char **argv) {
  const char *mode = getenv("PLUMOS_FRONTEND_MODE");
  const char *sd_root = getenv("PLUMOS_SDCARD_ROOT");
  const char *plumos_root = getenv("PLUMOS_ROOT");
  char log_path[PATH_MAX];
  struct log_ctx ctx;
  time_t now;
  int emu_configs;
  int rapp_configs;
  int app_configs;
  int theme_configs;
  char recent_path[PATH_MAX];

  (void)argc;
  (void)argv;

  if (!mode || !mode[0]) {
    mode = "boot";
  }
  if (!sd_root || !sd_root[0]) {
    sd_root = "/mnt/SDCARD";
  }
  if (!plumos_root || !plumos_root[0]) {
    plumos_root = "/mnt/SDCARD/plumos";
  }

  ensure_log_dir(plumos_root);
  snprintf(log_path, sizeof(log_path), "%s/logs/plumos-frontend.log", plumos_root);
  ctx.log = fopen(log_path, "a");
  ctx.stdout_enabled = strcmp(mode, "manual") == 0 || getenv("PLUMOS_FRONTEND_STDOUT") != NULL;

  now = time(NULL);
  emit(&ctx, "===== plumOS frontend prototype =====");
  emit(&ctx, "time=%ld mode=%s sd_root=%s plumos_root=%s", (long)now, mode, sd_root, plumos_root);

  emu_configs = scan_category(&ctx, sd_root, "Emu", "label");
  rapp_configs = scan_category(&ctx, sd_root, "RApp", "label");
  app_configs = scan_category(&ctx, sd_root, "App", "label");
  theme_configs = scan_category(&ctx, sd_root, "Themes", "name");

  snprintf(recent_path, sizeof(recent_path), "%s/Roms/recentlist.json", sd_root);
  emit(&ctx, "[recent] path=%s exists=%s", recent_path, is_file(recent_path) ? "yes" : "no");
  emit(&ctx, "summary emu=%d rapp=%d app=%d themes=%d", emu_configs, rapp_configs, app_configs,
       theme_configs);

  if (ctx.log) {
    fclose(ctx.log);
  }

  if (strcmp(mode, "manual") == 0) {
    return 0;
  }

  return 75;
}
