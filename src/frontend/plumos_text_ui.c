#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_TOP_ENTRIES 128
#define MAX_ROM_ENTRIES 4096
#define MAX_MENU_ENTRIES 64
#define MAX_APP_ENTRIES 128
#define TEXT_PATH_MAX 1024

struct top_entry {
  char id[64];
  char display_name[128];
  char short_name[64];
  char default_launch_profile[128];
  long rom_count;
  long thumbnail_count;
};

struct rom_entry {
  char id[64];
  char title[256];
  char file_name[256];
  char relative_path[TEXT_PATH_MAX];
  char path[TEXT_PATH_MAX];
  char thumbnail[TEXT_PATH_MAX];
};

struct menu_entry {
  char id[64];
  char display_name[128];
  char kind[64];
  char action[128];
  int confirm;
};

struct app_entry {
  char id[64];
  char display_name[128];
  char kind[64];
  char launch_profile[256];
  char menu[64];
  int visible;
};

static int copy_string(char *out, size_t out_size, const char *in) {
  size_t len;
  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!in) {
    return 0;
  }
  len = strlen(in);
  if (len + 1 > out_size) {
    return 0;
  }
  memcpy(out, in, len + 1);
  return 1;
}

static int append_string(char *out, size_t out_size, size_t *pos, const char *in) {
  size_t len;
  if (!out || !pos || !in) {
    return 0;
  }
  len = strlen(in);
  if (*pos + len + 1 > out_size) {
    return 0;
  }
  memcpy(out + *pos, in, len);
  *pos += len;
  out[*pos] = '\0';
  return 1;
}

static int join_path(char *out, size_t out_size, const char *a, const char *b) {
  size_t len_a;
  size_t len_b;
  size_t pos = 0;

  if (!out || out_size == 0 || !a || !b) {
    return 0;
  }
  out[0] = '\0';
  if (b[0] == '/') {
    return copy_string(out, out_size, b);
  }

  len_a = strlen(a);
  len_b = strlen(b);
  if (len_a + (len_a && a[len_a - 1] != '/' ? 1 : 0) + len_b + 1 > out_size) {
    return 0;
  }
  if (!append_string(out, out_size, &pos, a)) {
    return 0;
  }
  if (pos > 0 && out[pos - 1] != '/') {
    if (!append_string(out, out_size, &pos, "/")) {
      return 0;
    }
  }
  return append_string(out, out_size, &pos, b);
}

static int build_system_cache_path(char *out, size_t out_size, const char *plumos_root,
                                   const char *system_id) {
  char dir[PATH_MAX];
  char name[128];
  size_t id_len = strlen(system_id);

  if (id_len + 6 > sizeof(name)) {
    return 0;
  }
  memcpy(name, system_id, id_len);
  memcpy(name + id_len, ".json", 6);

  if (!join_path(dir, sizeof(dir), plumos_root, "state/frontend/systems")) {
    return 0;
  }
  return join_path(out, out_size, dir, name);
}

static int file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int valid_system_id(const char *s) {
  if (!s || !s[0]) {
    return 0;
  }
  while (*s) {
    unsigned char c = (unsigned char)*s++;
    if (!(isalnum(c) || c == '_' || c == '-')) {
      return 0;
    }
  }
  return 1;
}

static char *read_file(const char *path, size_t *size_out) {
  FILE *f;
  char *buf;
  long size;

  f = fopen(path, "rb");
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

static const char *skip_ws_range(const char *p, const char *end) {
  while (p < end && isspace((unsigned char)*p)) {
    p++;
  }
  return p;
}

static int json_decode_string(const char **cursor, const char *end, char *out, size_t out_size) {
  const char *p = *cursor;
  size_t n = 0;

  if (p >= end || *p != '"' || out_size == 0) {
    return 0;
  }
  p++;
  while (p < end && *p != '"') {
    char c = *p++;
    if (c == '\\' && p < end) {
      c = *p++;
      switch (c) {
      case '"':
      case '\\':
      case '/':
        break;
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      default:
        break;
      }
    }
    if (n + 1 < out_size) {
      out[n++] = c;
    }
  }
  if (p >= end || *p != '"') {
    out[0] = '\0';
    return 0;
  }
  out[n] = '\0';
  *cursor = p + 1;
  return 1;
}

static int json_match_container(const char *start, const char *end, char open_ch,
                                char close_ch, const char **body_start,
                                const char **body_end, const char **after_out) {
  const char *p = start;
  int depth = 0;
  int in_string = 0;
  int escape = 0;

  if (p >= end || *p != open_ch) {
    return 0;
  }

  while (p < end) {
    if (escape) {
      escape = 0;
      p++;
      continue;
    }
    if (in_string && *p == '\\') {
      escape = 1;
      p++;
      continue;
    }
    if (*p == '"') {
      in_string = !in_string;
      p++;
      continue;
    }
    if (!in_string && *p == open_ch) {
      if (depth == 0 && body_start) {
        *body_start = p + 1;
      }
      depth++;
    } else if (!in_string && *p == close_ch) {
      depth--;
      if (depth == 0) {
        if (body_end) {
          *body_end = p;
        }
        if (after_out) {
          *after_out = p + 1;
        }
        return 1;
      }
    }
    p++;
  }
  return 0;
}

static int json_find_key_value(const char *json, const char *end, const char *key,
                               const char **value_out) {
  char pattern[128];
  const char *p = json;

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  while (p < end && (p = strstr(p, pattern)) != NULL && p < end) {
    const char *q = p + strlen(pattern);
    q = skip_ws_range(q, end);
    if (q < end && *q == ':') {
      q++;
      *value_out = skip_ws_range(q, end);
      return 1;
    }
    p++;
  }
  return 0;
}

static int json_find_array(const char *json, const char *end, const char *key,
                           const char **body_start, const char **body_end) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return 0;
  }
  return json_match_container(value, end, '[', ']', body_start, body_end, NULL);
}

static int json_find_object(const char *json, const char *end, const char *key,
                            const char **body_start, const char **body_end) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return 0;
  }
  return json_match_container(value, end, '{', '}', body_start, body_end, NULL);
}

static int json_next_object(const char **cursor, const char *end,
                            const char **object_start, const char **object_end) {
  const char *p = *cursor;
  const char *body_start;
  const char *body_end;
  const char *after;

  while (p < end && *p != '{') {
    p++;
  }
  if (p >= end) {
    return 0;
  }
  if (!json_match_container(p, end, '{', '}', &body_start, &body_end, &after)) {
    return 0;
  }
  (void)body_start;
  *object_start = p;
  *object_end = after;
  *cursor = after;
  return 1;
}

static char *range_dup(const char *start, const char *end) {
  size_t len = (size_t)(end - start);
  char *s = (char *)calloc(len + 1, 1);
  if (!s) {
    return NULL;
  }
  memcpy(s, start, len);
  s[len] = '\0';
  return s;
}

static int json_get_string(const char *json, const char *end, const char *key,
                           char *out, size_t out_size) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  if (value < end && strncmp(value, "null", 4) == 0) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  return json_decode_string(&value, end, out, out_size);
}

static long json_get_long(const char *json, const char *end, const char *key, long default_value) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return default_value;
  }
  return strtol(value, NULL, 10);
}

static int json_get_bool(const char *json, const char *end, const char *key, int default_value) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return default_value;
  }
  if ((size_t)(end - value) >= 4 && strncmp(value, "true", 4) == 0) {
    return 1;
  }
  if ((size_t)(end - value) >= 5 && strncmp(value, "false", 5) == 0) {
    return 0;
  }
  return default_value;
}

static int run_scanner(const char *plumos_root, const char *sdcard_root, const char *system_id,
                       int full_refresh) {
  char scanner[PATH_MAX];
  char cmd[PATH_MAX * 3];
  int rc;

  if (!join_path(scanner, sizeof(scanner), plumos_root, "bin/plumos-library-scan")) {
    fprintf(stderr, "error: scanner path is too long\n");
    return 0;
  }
  if (!file_exists(scanner)) {
    fprintf(stderr, "error: missing scanner: %s\n", scanner);
    return 0;
  }
  if (system_id && !valid_system_id(system_id)) {
    fprintf(stderr, "error: invalid system id: %s\n", system_id);
    return 0;
  }

  if (full_refresh) {
    snprintf(cmd, sizeof(cmd),
             "PLUMOS_SDCARD_ROOT='%s' PLUMOS_ROOT='%s' '%s' >/dev/null",
             sdcard_root, plumos_root, scanner);
  } else {
    snprintf(cmd, sizeof(cmd),
             "PLUMOS_SDCARD_ROOT='%s' PLUMOS_ROOT='%s' '%s' --on-enter '%s' >/dev/null",
             sdcard_root, plumos_root, scanner, system_id);
  }

  rc = system(cmd);
  if (rc == -1) {
    fprintf(stderr, "error: failed to run scanner\n");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    return 1;
  }
  fprintf(stderr, "error: scanner failed with status %d\n", rc);
  return 0;
}

static int load_top_entries(const char *path, struct top_entry *entries, size_t max_entries,
                            size_t *count_out, long *ready_ms_out) {
  char *json;
  size_t json_size;
  const char *systems_start;
  const char *systems_end;
  const char *cursor;
  size_t count = 0;

  *count_out = 0;
  *ready_ms_out = -1;
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  *ready_ms_out = json_get_long(json, json + json_size, "ready_ms", -1);
  if (!json_find_array(json, json + json_size, "systems", &systems_start, &systems_end)) {
    free(json);
    return 0;
  }

  cursor = systems_start;
  while (count < max_entries) {
    const char *obj_start;
    const char *obj_end;
    char *obj;
    struct top_entry entry;

    if (!json_next_object(&cursor, systems_end, &obj_start, &obj_end)) {
      break;
    }
    obj = range_dup(obj_start, obj_end);
    if (!obj) {
      free(json);
      return 0;
    }

    memset(&entry, 0, sizeof(entry));
    json_get_string(obj, obj + strlen(obj), "id", entry.id, sizeof(entry.id));
    json_get_string(obj, obj + strlen(obj), "display_name", entry.display_name,
                    sizeof(entry.display_name));
    json_get_string(obj, obj + strlen(obj), "short_name", entry.short_name,
                    sizeof(entry.short_name));
    json_get_string(obj, obj + strlen(obj), "default_launch_profile",
                    entry.default_launch_profile, sizeof(entry.default_launch_profile));
    entry.rom_count = json_get_long(obj, obj + strlen(obj), "rom_count", 0);
    entry.thumbnail_count = json_get_long(obj, obj + strlen(obj), "thumbnail_count", 0);

    if (entry.id[0]) {
      if (!entry.display_name[0]) {
        copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
      }
      entries[count++] = entry;
    }
    free(obj);
  }

  *count_out = count;
  free(json);
  return 1;
}

static int load_rom_entries(const char *path, const char *system_id, struct rom_entry *entries,
                            size_t max_entries, size_t *count_out, long *ready_ms_out) {
  char *json;
  size_t json_size;
  const char *systems_start;
  const char *systems_end;
  const char *system_cursor;
  size_t count = 0;

  *count_out = 0;
  *ready_ms_out = -1;
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  *ready_ms_out = json_get_long(json, json + json_size, "ready_ms", -1);
  if (!json_find_array(json, json + json_size, "systems", &systems_start, &systems_end)) {
    free(json);
    return 0;
  }

  system_cursor = systems_start;
  while (1) {
    const char *system_start;
    const char *system_end;
    char *system_obj;
    char id[64];
    const char *roms_start;
    const char *roms_end;
    const char *rom_cursor;

    if (!json_next_object(&system_cursor, systems_end, &system_start, &system_end)) {
      break;
    }
    system_obj = range_dup(system_start, system_end);
    if (!system_obj) {
      free(json);
      return 0;
    }
    json_get_string(system_obj, system_obj + strlen(system_obj), "id", id, sizeof(id));
    if (strcmp(id, system_id) != 0) {
      free(system_obj);
      continue;
    }

    if (!json_find_array(system_obj, system_obj + strlen(system_obj), "roms", &roms_start,
                         &roms_end)) {
      free(system_obj);
      free(json);
      return 0;
    }

    rom_cursor = roms_start;
    while (count < max_entries) {
      const char *rom_start;
      const char *rom_end;
      const char *media_start;
      const char *media_end;
      char *rom_obj;
      struct rom_entry entry;

      if (!json_next_object(&rom_cursor, roms_end, &rom_start, &rom_end)) {
        break;
      }
      rom_obj = range_dup(rom_start, rom_end);
      if (!rom_obj) {
        free(system_obj);
        free(json);
        return 0;
      }

      memset(&entry, 0, sizeof(entry));
      json_get_string(rom_obj, rom_obj + strlen(rom_obj), "id", entry.id, sizeof(entry.id));
      json_get_string(rom_obj, rom_obj + strlen(rom_obj), "title", entry.title,
                      sizeof(entry.title));
      json_get_string(rom_obj, rom_obj + strlen(rom_obj), "file_name", entry.file_name,
                      sizeof(entry.file_name));
      json_get_string(rom_obj, rom_obj + strlen(rom_obj), "relative_path", entry.relative_path,
                      sizeof(entry.relative_path));
      json_get_string(rom_obj, rom_obj + strlen(rom_obj), "path", entry.path, sizeof(entry.path));
      if (json_find_object(rom_obj, rom_obj + strlen(rom_obj), "media", &media_start,
                           &media_end)) {
        char *media_obj = range_dup(media_start, media_end);
        if (media_obj) {
          json_get_string(media_obj, media_obj + strlen(media_obj), "thumbnail", entry.thumbnail,
                          sizeof(entry.thumbnail));
          free(media_obj);
        }
      }
      if (!entry.title[0]) {
        copy_string(entry.title, sizeof(entry.title),
                    entry.file_name[0] ? entry.file_name : entry.relative_path);
      }
      entries[count++] = entry;
      free(rom_obj);
    }

    free(system_obj);
    break;
  }

  *count_out = count;
  free(json);
  return 1;
}

static int load_menu_entries(const char *path, const char *menu_id, struct menu_entry *entries,
                             size_t max_entries, size_t *count_out) {
  char *json;
  size_t json_size;
  const char *menus_start;
  const char *menus_end;
  const char *menu_cursor;
  size_t count = 0;

  *count_out = 0;
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "menus", &menus_start, &menus_end)) {
    free(json);
    return 0;
  }

  menu_cursor = menus_start;
  while (1) {
    const char *menu_start;
    const char *menu_end;
    const char *entries_start;
    const char *entries_end;
    const char *entry_cursor;
    char *menu_obj;
    char id[64];

    if (!json_next_object(&menu_cursor, menus_end, &menu_start, &menu_end)) {
      break;
    }
    menu_obj = range_dup(menu_start, menu_end);
    if (!menu_obj) {
      free(json);
      return 0;
    }
    json_get_string(menu_obj, menu_obj + strlen(menu_obj), "id", id, sizeof(id));
    if (strcmp(id, menu_id) != 0) {
      free(menu_obj);
      continue;
    }

    if (!json_find_array(menu_obj, menu_obj + strlen(menu_obj), "entries", &entries_start,
                         &entries_end)) {
      free(menu_obj);
      free(json);
      return 0;
    }

    entry_cursor = entries_start;
    while (count < max_entries) {
      const char *entry_start;
      const char *entry_end;
      char *entry_obj;
      struct menu_entry entry;

      if (!json_next_object(&entry_cursor, entries_end, &entry_start, &entry_end)) {
        break;
      }
      entry_obj = range_dup(entry_start, entry_end);
      if (!entry_obj) {
        free(menu_obj);
        free(json);
        return 0;
      }

      memset(&entry, 0, sizeof(entry));
      json_get_string(entry_obj, entry_obj + strlen(entry_obj), "id", entry.id,
                      sizeof(entry.id));
      json_get_string(entry_obj, entry_obj + strlen(entry_obj), "display_name",
                      entry.display_name, sizeof(entry.display_name));
      json_get_string(entry_obj, entry_obj + strlen(entry_obj), "kind", entry.kind,
                      sizeof(entry.kind));
      json_get_string(entry_obj, entry_obj + strlen(entry_obj), "action", entry.action,
                      sizeof(entry.action));
      entry.confirm = json_get_bool(entry_obj, entry_obj + strlen(entry_obj), "confirm", 0);
      if (entry.id[0]) {
        if (!entry.display_name[0]) {
          copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
        }
        entries[count++] = entry;
      }
      free(entry_obj);
    }

    free(menu_obj);
    break;
  }

  *count_out = count;
  free(json);
  return count > 0;
}

static int load_app_entries(const char *path, const char *menu_id, struct app_entry *entries,
                            size_t max_entries, size_t *count_out) {
  char *json;
  size_t json_size;
  const char *apps_start;
  const char *apps_end;
  const char *app_cursor;
  size_t count = 0;

  *count_out = 0;
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "apps", &apps_start, &apps_end)) {
    free(json);
    return 0;
  }

  app_cursor = apps_start;
  while (count < max_entries) {
    const char *app_start;
    const char *app_end;
    char *app_obj;
    struct app_entry entry;

    if (!json_next_object(&app_cursor, apps_end, &app_start, &app_end)) {
      break;
    }
    app_obj = range_dup(app_start, app_end);
    if (!app_obj) {
      free(json);
      return 0;
    }

    memset(&entry, 0, sizeof(entry));
    entry.visible = json_get_bool(app_obj, app_obj + strlen(app_obj), "visible", 1);
    json_get_string(app_obj, app_obj + strlen(app_obj), "id", entry.id, sizeof(entry.id));
    json_get_string(app_obj, app_obj + strlen(app_obj), "display_name", entry.display_name,
                    sizeof(entry.display_name));
    json_get_string(app_obj, app_obj + strlen(app_obj), "kind", entry.kind, sizeof(entry.kind));
    json_get_string(app_obj, app_obj + strlen(app_obj), "launch_profile", entry.launch_profile,
                    sizeof(entry.launch_profile));
    json_get_string(app_obj, app_obj + strlen(app_obj), "menu", entry.menu, sizeof(entry.menu));

    if (entry.visible && entry.id[0] && strcmp(entry.menu, menu_id) == 0) {
      if (!entry.display_name[0]) {
        copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
      }
      entries[count++] = entry;
    }
    free(app_obj);
  }

  *count_out = count;
  free(json);
  return 1;
}

static void print_top(const struct top_entry *entries, size_t count, int show_all, size_t limit,
                      const char *cache_path, long ready_ms) {
  size_t i;
  size_t shown = 0;
  size_t available = 0;

  printf("plumOS text UI - TOP\n");
  printf("cache: %s\n", cache_path);
  if (ready_ms >= 0) {
    printf("cache_ready_ms: %ld\n", ready_ms);
  }
  printf("\n");
  printf("%-4s %-18s %8s  %s\n", "No.", "System", "ROMs", "Default profile");
  printf("%-4s %-18s %8s  %s\n", "---", "------", "----", "---------------");

  for (i = 0; i < count; i++) {
    if (!show_all && entries[i].rom_count <= 0) {
      continue;
    }
    available++;
    if (limit > 0 && shown >= limit) {
      continue;
    }
    shown++;
    printf("%3zu. %-18s %8ld  %s\n", shown, entries[i].display_name, entries[i].rom_count,
           entries[i].default_launch_profile);
  }
  if (available == 0) {
    printf("(ROMがあるsystemはまだありません。full scan cacheを確認してください。)\n");
  } else if (limit > 0 && available > shown) {
    printf("... %zu more\n", available - shown);
  }
}

static void print_roms(const char *system_id, const struct rom_entry *entries, size_t count,
                       size_t limit, const char *cache_path, long ready_ms) {
  size_t i;
  size_t shown = 0;

  printf("plumOS text UI - ROM list\n");
  printf("system: %s\n", system_id);
  printf("cache: %s\n", cache_path);
  if (ready_ms >= 0) {
    printf("ready_ms: %ld\n", ready_ms);
  }
  printf("\n");
  printf("%-4s %-34s %s\n", "No.", "Title", "Path");
  printf("%-4s %-34s %s\n", "---", "-----", "----");

  for (i = 0; i < count; i++) {
    if (limit > 0 && shown >= limit) {
      continue;
    }
    shown++;
    printf("%3zu. %-34s %s\n", shown, entries[i].title, entries[i].relative_path);
  }
  if (count == 0) {
    printf("(このsystemには表示できるROMがありません。)\n");
  } else if (limit > 0 && count > shown) {
    printf("... %zu more\n", count - shown);
  }
}

static void print_menu(const char *menu_id, const struct menu_entry *entries, size_t count,
                       size_t limit, const char *path) {
  size_t i;
  size_t shown = 0;

  printf("plumOS text UI - menu\n");
  printf("menu: %s\n", menu_id);
  printf("source: %s\n", path);
  printf("\n");
  printf("%-4s %-24s %-10s %-24s %s\n", "No.", "Entry", "Kind", "Action", "Confirm");
  printf("%-4s %-24s %-10s %-24s %s\n", "---", "-----", "----", "------", "-------");

  for (i = 0; i < count; i++) {
    if (limit > 0 && shown >= limit) {
      continue;
    }
    shown++;
    printf("%3zu. %-24s %-10s %-24s %s\n", shown, entries[i].display_name,
           entries[i].kind[0] ? entries[i].kind : "-", entries[i].action,
           entries[i].confirm ? "yes" : "no");
  }
  if (count == 0) {
    printf("(menu entry はありません。)\n");
  } else if (limit > 0 && count > shown) {
    printf("... %zu more\n", count - shown);
  }
}

static void print_apps_menu(const char *menu_id, const struct app_entry *entries, size_t count,
                            size_t limit, const char *path) {
  size_t i;
  size_t shown = 0;

  printf("plumOS text UI - apps menu\n");
  printf("menu: %s\n", menu_id);
  printf("source: %s\n", path);
  printf("\n");
  printf("%-4s %-24s %-10s %s\n", "No.", "App", "Kind", "Launch profile");
  printf("%-4s %-24s %-10s %s\n", "---", "---", "----", "--------------");

  for (i = 0; i < count; i++) {
    if (limit > 0 && shown >= limit) {
      continue;
    }
    shown++;
    printf("%3zu. %-24s %-10s %s\n", shown, entries[i].display_name,
           entries[i].kind[0] ? entries[i].kind : "-", entries[i].launch_profile);
  }
  if (count == 0) {
    printf("(app/tool entry はありません。)\n");
  } else if (limit > 0 && count > shown) {
    printf("... %zu more\n", count - shown);
  }
}

static void usage(const char *argv0) {
  printf("Usage:\n");
  printf("  %s top [--all] [--refresh] [--limit N]\n", argv0);
  printf("  %s roms SYSTEM [--no-scan] [--limit N]\n", argv0);
  printf("  %s menu start|apps [--limit N]\n", argv0);
  printf("\n");
  printf("Environment:\n");
  printf("  PLUMOS_SDCARD_ROOT  Default: /mnt/SDCARD\n");
  printf("  PLUMOS_ROOT         Default: $PLUMOS_SDCARD_ROOT/plumos\n");
  printf("  PLUMOS_TEXT_LIMIT   Default: 50 for ROM lists, unlimited for TOP\n");
}

int main(int argc, char **argv) {
  const char *sdcard_root;
  const char *plumos_root_env;
  char plumos_root[PATH_MAX];
  char full_cache_path[PATH_MAX];
  char system_cache_path[PATH_MAX];
  char menus_path[PATH_MAX];
  char apps_path[PATH_MAX];
  const char *cmd;
  size_t limit = 0;
  int i;

  sdcard_root = getenv("PLUMOS_SDCARD_ROOT");
  if (!sdcard_root || !sdcard_root[0]) {
    sdcard_root = "/mnt/SDCARD";
  }
  plumos_root_env = getenv("PLUMOS_ROOT");
  if (plumos_root_env && plumos_root_env[0]) {
    copy_string(plumos_root, sizeof(plumos_root), plumos_root_env);
  } else if (!join_path(plumos_root, sizeof(plumos_root), sdcard_root, "plumos")) {
    fprintf(stderr, "error: plumOS root path is too long\n");
    return 1;
  }
  if (!join_path(full_cache_path, sizeof(full_cache_path), plumos_root,
                 "state/frontend/library-index.json")) {
    fprintf(stderr, "error: library-index path is too long\n");
    return 1;
  }
  if (!join_path(menus_path, sizeof(menus_path), plumos_root, "config/frontend/menus.json") ||
      !join_path(apps_path, sizeof(apps_path), plumos_root, "config/frontend/apps.json")) {
    fprintf(stderr, "error: menu config path is too long\n");
    return 1;
  }

  if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
    usage(argv[0]);
    return argc < 2 ? 1 : 0;
  }

  cmd = argv[1];
  if (strcmp(cmd, "top") == 0) {
    struct top_entry *entries;
    size_t count = 0;
    long ready_ms = -1;
    int show_all = 0;
    int refresh = 0;

    for (i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--all") == 0) {
        show_all = 1;
      } else if (strcmp(argv[i], "--refresh") == 0) {
        refresh = 1;
      } else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
        limit = (size_t)strtoul(argv[++i], NULL, 10);
      } else {
        fprintf(stderr, "error: unknown top option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    entries = (struct top_entry *)calloc(MAX_TOP_ENTRIES, sizeof(entries[0]));
    if (!entries) {
      fprintf(stderr, "error: out of memory\n");
      return 1;
    }
    if ((refresh || !file_exists(full_cache_path)) &&
        !run_scanner(plumos_root, sdcard_root, NULL, 1)) {
      free(entries);
      return 1;
    }
    if (!load_top_entries(full_cache_path, entries, MAX_TOP_ENTRIES, &count, &ready_ms)) {
      fprintf(stderr, "error: cannot read top cache: %s\n", full_cache_path);
      free(entries);
      return 1;
    }
    print_top(entries, count, show_all, limit, full_cache_path, ready_ms);
    free(entries);
    return 0;
  }

  if (strcmp(cmd, "roms") == 0) {
    struct rom_entry *entries;
    const char *system_id;
    size_t count = 0;
    long ready_ms = -1;
    int scan = 1;
    const char *limit_env = getenv("PLUMOS_TEXT_LIMIT");

    if (argc < 3) {
      fprintf(stderr, "error: roms requires SYSTEM\n");
      usage(argv[0]);
      return 2;
    }
    system_id = argv[2];
    if (!valid_system_id(system_id)) {
      fprintf(stderr, "error: invalid system id: %s\n", system_id);
      return 2;
    }
    limit = limit_env && limit_env[0] ? (size_t)strtoul(limit_env, NULL, 10) : 50;

    for (i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--no-scan") == 0) {
        scan = 0;
      } else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
        limit = (size_t)strtoul(argv[++i], NULL, 10);
      } else {
        fprintf(stderr, "error: unknown roms option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    entries = (struct rom_entry *)calloc(MAX_ROM_ENTRIES, sizeof(entries[0]));
    if (!entries) {
      fprintf(stderr, "error: out of memory\n");
      return 1;
    }
    if (!build_system_cache_path(system_cache_path, sizeof(system_cache_path), plumos_root,
                                 system_id)) {
      fprintf(stderr, "error: system cache path is too long\n");
      free(entries);
      return 1;
    }
    if ((scan || !file_exists(system_cache_path)) &&
        !run_scanner(plumos_root, sdcard_root, system_id, 0)) {
      free(entries);
      return 1;
    }
    if (!load_rom_entries(system_cache_path, system_id, entries, MAX_ROM_ENTRIES, &count,
                          &ready_ms)) {
      fprintf(stderr, "error: cannot read ROM cache: %s\n", system_cache_path);
      free(entries);
      return 1;
    }
    print_roms(system_id, entries, count, limit, system_cache_path, ready_ms);
    free(entries);
    return 0;
  }

  if (strcmp(cmd, "menu") == 0) {
    const char *menu_id;

    if (argc < 3) {
      fprintf(stderr, "error: menu requires start or apps\n");
      usage(argv[0]);
      return 2;
    }
    menu_id = argv[2];
    if (!valid_system_id(menu_id)) {
      fprintf(stderr, "error: invalid menu id: %s\n", menu_id);
      return 2;
    }
    for (i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
        limit = (size_t)strtoul(argv[++i], NULL, 10);
      } else {
        fprintf(stderr, "error: unknown menu option: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    if (strcmp(menu_id, "apps") == 0) {
      struct app_entry *entries;
      size_t count = 0;

      entries = (struct app_entry *)calloc(MAX_APP_ENTRIES, sizeof(entries[0]));
      if (!entries) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
      }
      if (!load_app_entries(apps_path, menu_id, entries, MAX_APP_ENTRIES, &count)) {
        fprintf(stderr, "error: cannot read apps menu: %s\n", apps_path);
        free(entries);
        return 1;
      }
      print_apps_menu(menu_id, entries, count, limit, apps_path);
      free(entries);
      return 0;
    } else {
      struct menu_entry *entries;
      size_t count = 0;

      entries = (struct menu_entry *)calloc(MAX_MENU_ENTRIES, sizeof(entries[0]));
      if (!entries) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
      }
      if (!load_menu_entries(menus_path, menu_id, entries, MAX_MENU_ENTRIES, &count)) {
        fprintf(stderr, "error: cannot read menu: %s id=%s\n", menus_path, menu_id);
        free(entries);
        return 1;
      }
      print_menu(menu_id, entries, count, limit, menus_path);
      free(entries);
      return 0;
    }
  }

  fprintf(stderr, "error: unknown command: %s\n", cmd);
  usage(argv[0]);
  return 2;
}
