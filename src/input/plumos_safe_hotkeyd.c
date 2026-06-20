#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<linux/input.h>)
#include <linux/input.h>
#define PLUMOS_HAS_INPUT 1
#endif
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef PLUMOS_HAS_INPUT
#define EV_KEY 0x01
struct input_event {
  struct timeval time;
  unsigned short type;
  unsigned short code;
  int value;
};
#endif

#ifndef KEY_ESC
#define KEY_ESC 1
#endif
#ifndef KEY_VOLUMEDOWN
#define KEY_VOLUMEDOWN 114
#endif
#ifndef KEY_VOLUMEUP
#define KEY_VOLUMEUP 115
#endif
#ifndef KEY_POWER
#define KEY_POWER 116
#endif

#define DEFAULT_ROOT "/mnt/SDCARD/plumos"
#define DEFAULT_SDCARD_ROOT "/mnt/SDCARD"
#define DEFAULT_DEBOUNCE_MS 1500
#define DEFAULT_POLL_MS 250
#define VOLUME_LOCK_PATH "/tmp/plumos-safe-hotkeyd-volume.lock"
#define POWER_OVERLAY_PIDS_ENV "PLUMOS_POWER_MENU_OVERLAY_PIDS"
#define COMMAND_MAX 8192
#define SHELL_QUOTE_MAX (PATH_MAX * 4 + 4)

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_trigger_signal = 0;

struct config {
  char root[PATH_MAX];
  char sdcard_root[PATH_MAX];
  char event_path[PATH_MAX];
  char volume_event_path[PATH_MAX];
  char log_path[PATH_MAX];
  char command[COMMAND_MAX];
  char action[16];
  int command_set;
  int poweroff;
  int key_code;
  int timeout_ms;
  int debounce_ms;
  int oneshot;
  int dry_run;
  int trigger_now;
  int verbose;
  int safe_key_enabled;
  int volume_keys_enabled;
  int event_path_explicit;
  int volume_event_path_explicit;
  int volume_lock_fd;
};

static void handle_signal(int sig) {
  if (sig == SIGUSR1) {
    g_trigger_signal = 1;
  } else {
    g_stop = 1;
  }
}

static long long now_ms(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    return 0;
  }
  return ((long long)tv.tv_sec * 1000LL) + ((long long)tv.tv_usec / 1000LL);
}

static int copy_string(char *out, size_t out_size, const char *value) {
  size_t len;

  if (!out || out_size == 0 || !value) {
    return 0;
  }
  len = strlen(value);
  if (len + 1 > out_size) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  memcpy(out, value, len + 1);
  return 1;
}

static int join_path(char *out, size_t out_size, const char *base, const char *leaf) {
  size_t base_len;
  size_t leaf_len;
  int need_slash;

  if (!out || out_size == 0 || !base || !leaf) {
    return 0;
  }
  base_len = strlen(base);
  leaf_len = strlen(leaf);
  need_slash = (base_len > 0 && base[base_len - 1] != '/');
  if (base_len + (size_t)need_slash + leaf_len + 1 > out_size) {
    out[0] = '\0';
    return 0;
  }
  memcpy(out, base, base_len);
  if (need_slash) {
    out[base_len++] = '/';
  }
  memcpy(out + base_len, leaf, leaf_len + 1);
  return 1;
}

static int mkdir_p(const char *path) {
  char tmp[PATH_MAX];
  size_t len;
  char *p;

  if (!path || !path[0]) {
    return 0;
  }
  if (!copy_string(tmp, sizeof(tmp), path)) {
    return 0;
  }
  len = strlen(tmp);
  while (len > 1 && tmp[len - 1] == '/') {
    tmp[--len] = '\0';
  }
  for (p = tmp + 1; *p; p++) {
    if (*p != '/') {
      continue;
    }
    *p = '\0';
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
      return 0;
    }
    *p = '/';
  }
  if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
    return 0;
  }
  return 1;
}

static void ensure_parent_dir(const char *path) {
  char tmp[PATH_MAX];
  char *slash;

  if (!path || !copy_string(tmp, sizeof(tmp), path)) {
    return;
  }
  slash = strrchr(tmp, '/');
  if (!slash || slash == tmp) {
    return;
  }
  *slash = '\0';
  mkdir_p(tmp);
}

static void timestamp_now(char *out, size_t out_size) {
  time_t t;
  struct tm tmv;

  if (!out || out_size == 0) {
    return;
  }
  t = time(NULL);
  if (t == (time_t)-1 || !localtime_r(&t, &tmv) ||
      strftime(out, out_size, "%Y-%m-%d %H:%M:%S", &tmv) == 0) {
    copy_string(out, out_size, "unknown-time");
  }
}

static void vlog_msg(const struct config *cfg, int to_stdout, const char *fmt, va_list ap) {
  FILE *f;
  char ts[32];
  va_list ap_copy;

  timestamp_now(ts, sizeof(ts));
  if (cfg && cfg->log_path[0]) {
    f = fopen(cfg->log_path, "a");
    if (f) {
      fprintf(f, "%s ", ts);
      va_copy(ap_copy, ap);
      vfprintf(f, fmt, ap_copy);
      va_end(ap_copy);
      fprintf(f, "\n");
      fclose(f);
    }
  }
  if (to_stdout) {
    printf("%s ", ts);
    vprintf(fmt, ap);
    printf("\n");
    fflush(stdout);
  }
}

static void log_msg(const struct config *cfg, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vlog_msg(cfg, cfg ? cfg->verbose : 0, fmt, ap);
  va_end(ap);
}

static void emit_msg(const struct config *cfg, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vlog_msg(cfg, 1, fmt, ap);
  va_end(ap);
}

static int shell_quote(char *out, size_t out_size, const char *value) {
  size_t pos = 0;
  size_t i;

  if (!out || out_size < 3 || !value) {
    return 0;
  }
  out[pos++] = '\'';
  for (i = 0; value[i]; i++) {
    if (value[i] == '\'') {
      const char *escaped = "'\\''";
      size_t n = strlen(escaped);
      if (pos + n >= out_size) {
        out[0] = '\0';
        return 0;
      }
      memcpy(out + pos, escaped, n);
      pos += n;
    } else {
      if (pos + 1 >= out_size) {
        out[0] = '\0';
        return 0;
      }
      out[pos++] = value[i];
    }
  }
  if (pos + 2 > out_size) {
    out[0] = '\0';
    return 0;
  }
  out[pos++] = '\'';
  out[pos] = '\0';
  return 1;
}

static int parse_int_arg(const char *name, const char *value, int min, int max, int *out) {
  char *end = NULL;
  long n;

  errno = 0;
  n = strtol(value, &end, 0);
  if (errno != 0 || end == value || *end != '\0' || n < min || n > max) {
    fprintf(stderr, "error: %s expects an integer in range %d..%d: %s\n", name, min, max,
            value ? value : "(null)");
    return 0;
  }
  *out = (int)n;
  return 1;
}

static int parse_action(const char *value, char *out, size_t out_size) {
  if (strcmp(value, "shutdown") == 0 || strcmp(value, "sleep") == 0) {
    return copy_string(out, out_size, value);
  }
  fprintf(stderr, "error: invalid action: %s\n", value);
  return 0;
}

static int discover_named_input_event(char *out, size_t out_size, const char *target_name,
                                      const char *fallback_path) {
  FILE *f;
  char line[512];
  int in_target = 0;

  f = fopen("/proc/bus/input/devices", "rb");
  if (!f) {
    return copy_string(out, out_size, fallback_path);
  }
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '\n') {
      in_target = 0;
      continue;
    }
    if (strstr(line, "Name=\"") && strstr(line, target_name)) {
      in_target = 1;
      continue;
    }
    if (in_target && strstr(line, "Handlers=")) {
      char *event = strstr(line, "event");
      if (event) {
        char name[32];
        size_t n = 0;
        while (event[n] && !isspace((unsigned char)event[n]) && n + 1 < sizeof(name)) {
          name[n] = event[n];
          n++;
        }
        name[n] = '\0';
        fclose(f);
        return join_path(out, out_size, "/dev/input", name);
      }
    }
  }
  fclose(f);
  return copy_string(out, out_size, fallback_path);
}

static int discover_input_event(char *out, size_t out_size, int key_code) {
  if (key_code == KEY_POWER) {
    return discover_named_input_event(out, out_size, "axp22-supplyer", "/dev/input/event0");
  }
  return discover_named_input_event(out, out_size, "gpio-keys-polled", "/dev/input/event3");
}

static int discover_volume_input_event(char *out, size_t out_size) {
  return discover_named_input_event(out, out_size, "gpio-keys-polled", "/dev/input/event3");
}

static int same_path(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

static int string_is_pid(const char *s) {
  const unsigned char *p = (const unsigned char *)s;

  if (!p || !*p) {
    return 0;
  }
  while (*p) {
    if (!isdigit(*p)) {
      return 0;
    }
    p++;
  }
  return 1;
}

static int pid_list_contains(const char *list, const char *pid) {
  size_t pid_len;
  const char *p;

  if (!list || !pid || !pid[0]) {
    return 0;
  }
  pid_len = strlen(pid);
  p = list;
  while (*p) {
    while (*p == ' ') {
      p++;
    }
    if (strncmp(p, pid, pid_len) == 0 && (p[pid_len] == '\0' || p[pid_len] == ' ')) {
      return 1;
    }
    while (*p && *p != ' ') {
      p++;
    }
  }
  return 0;
}

static int append_pid_to_list(char *out, size_t out_size, const char *pid) {
  size_t len;
  size_t pid_len;

  if (!out || out_size == 0 || !pid || !pid[0] || pid_list_contains(out, pid)) {
    return 1;
  }
  len = strlen(out);
  pid_len = strlen(pid);
  if (len + (len > 0 ? 1 : 0) + pid_len + 1 > out_size) {
    return 0;
  }
  if (len > 0) {
    out[len++] = ' ';
    out[len] = '\0';
  }
  memcpy(out + len, pid, pid_len + 1);
  return 1;
}

static void collect_fb0_owner_pids(char *out, size_t out_size) {
  DIR *proc;
  struct dirent *proc_entry;
  char self_pid[32];

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  snprintf(self_pid, sizeof(self_pid), "%ld", (long)getpid());
  proc = opendir("/proc");
  if (!proc) {
    return;
  }
  while ((proc_entry = readdir(proc)) != NULL) {
    char fd_dir_path[PATH_MAX];
    DIR *fd_dir;
    struct dirent *fd_entry;

    if (!string_is_pid(proc_entry->d_name) || strcmp(proc_entry->d_name, self_pid) == 0) {
      continue;
    }
    snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/%s/fd", proc_entry->d_name);
    fd_dir = opendir(fd_dir_path);
    if (!fd_dir) {
      continue;
    }
    while ((fd_entry = readdir(fd_dir)) != NULL) {
      char link_path[PATH_MAX];
      char target[PATH_MAX];
      size_t dir_len;
      size_t name_len;
      ssize_t n;

      if (fd_entry->d_name[0] == '.') {
        continue;
      }
      dir_len = strlen(fd_dir_path);
      name_len = strlen(fd_entry->d_name);
      if (dir_len + 1 + name_len + 1 > sizeof(link_path)) {
        continue;
      }
      memcpy(link_path, fd_dir_path, dir_len);
      link_path[dir_len] = '/';
      memcpy(link_path + dir_len + 1, fd_entry->d_name, name_len + 1);
      n = readlink(link_path, target, sizeof(target) - 1);
      if (n < 0) {
        continue;
      }
      target[n] = '\0';
      if (strcmp(target, "/dev/fb0") == 0) {
        append_pid_to_list(out, out_size, proc_entry->d_name);
        break;
      }
    }
    closedir(fd_dir);
  }
  closedir(proc);
}

static int build_default_command(struct config *cfg) {
  char bin[PATH_MAX];
  char command_log[PATH_MAX];
  char q_root[SHELL_QUOTE_MAX];
  char q_sdcard[SHELL_QUOTE_MAX];
  char q_bin[SHELL_QUOTE_MAX];
  char q_command_log[SHELL_QUOTE_MAX];
  int n;

  if (!join_path(bin, sizeof(bin), cfg->root, "bin/plumos-power-menu-overlay") ||
      !join_path(command_log, sizeof(command_log), cfg->root, "logs/safe-hotkeyd-command.log")) {
    fprintf(stderr, "error: plumOS path is too long\n");
    return 0;
  }
  if (!shell_quote(q_root, sizeof(q_root), cfg->root) ||
      !shell_quote(q_sdcard, sizeof(q_sdcard), cfg->sdcard_root) ||
      !shell_quote(q_bin, sizeof(q_bin), bin) ||
      !shell_quote(q_command_log, sizeof(q_command_log), command_log)) {
    fprintf(stderr, "error: shell command path is too long\n");
    return 0;
  }

  n = snprintf(cfg->command, sizeof(cfg->command),
               "PLUMOS_ROOT=%s PLUMOS_SDCARD_ROOT=%s %s >> %s 2>&1",
               q_root, q_sdcard, q_bin, q_command_log);
  if (n < 0 || (size_t)n >= sizeof(cfg->command)) {
    fprintf(stderr, "error: power menu command is too long\n");
    return 0;
  }
  return 1;
}

static int init_config(struct config *cfg) {
  const char *env;

  memset(cfg, 0, sizeof(*cfg));
  if (!copy_string(cfg->root, sizeof(cfg->root), DEFAULT_ROOT) ||
      !copy_string(cfg->sdcard_root, sizeof(cfg->sdcard_root), DEFAULT_SDCARD_ROOT) ||
      !copy_string(cfg->action, sizeof(cfg->action), "shutdown")) {
    return 0;
  }
  cfg->key_code = KEY_POWER;
  cfg->debounce_ms = DEFAULT_DEBOUNCE_MS;
  cfg->volume_lock_fd = -1;
  cfg->safe_key_enabled = 1;
  cfg->volume_keys_enabled = 1;

  env = getenv("PLUMOS_ROOT");
  if (env && env[0] && !copy_string(cfg->root, sizeof(cfg->root), env)) {
    return 0;
  }
  env = getenv("PLUMOS_SDCARD_ROOT");
  if (env && env[0] && !copy_string(cfg->sdcard_root, sizeof(cfg->sdcard_root), env)) {
    return 0;
  }
  env = getenv("PLUMOS_SAFE_HOTKEY_KEY_CODE");
  if (env && env[0] && !parse_int_arg("PLUMOS_SAFE_HOTKEY_KEY_CODE", env, 0, 1024,
                                      &cfg->key_code)) {
    return 0;
  }
  env = getenv("PLUMOS_SAFE_HOTKEYD_SAFE_KEY");
  if (env && env[0]) {
    cfg->safe_key_enabled =
        !(strcmp(env, "0") == 0 || strcmp(env, "false") == 0 ||
          strcmp(env, "False") == 0 || strcmp(env, "no") == 0 ||
          strcmp(env, "No") == 0 || strcmp(env, "off") == 0 ||
          strcmp(env, "Off") == 0);
  }
  env = getenv("PLUMOS_SAFE_HOTKEY_EVENT");
  if (env && env[0]) {
    if (!copy_string(cfg->event_path, sizeof(cfg->event_path), env)) {
      return 0;
    }
    cfg->event_path_explicit = 1;
  } else if (cfg->safe_key_enabled &&
             !discover_input_event(cfg->event_path, sizeof(cfg->event_path), cfg->key_code)) {
    return 0;
  }
  env = getenv("PLUMOS_SAFE_HOTKEY_VOLUME_EVENT");
  if (env && env[0]) {
    if (!copy_string(cfg->volume_event_path, sizeof(cfg->volume_event_path), env)) {
      return 0;
    }
    cfg->volume_event_path_explicit = 1;
  } else if (!discover_volume_input_event(cfg->volume_event_path, sizeof(cfg->volume_event_path))) {
    return 0;
  }
  env = getenv("PLUMOS_SAFE_HOTKEY_LOG");
  if (env && env[0]) {
    if (!copy_string(cfg->log_path, sizeof(cfg->log_path), env)) {
      return 0;
    }
  } else if (!join_path(cfg->log_path, sizeof(cfg->log_path), cfg->root,
                       "logs/safe-hotkeyd.log")) {
    return 0;
  }
  env = getenv("PLUMOS_SAFE_HOTKEY_COMMAND");
  if (env && env[0]) {
    if (!copy_string(cfg->command, sizeof(cfg->command), env)) {
      return 0;
    }
    cfg->command_set = 1;
  }
  env = getenv("PLUMOS_SAFE_HOTKEY_TIMEOUT_MS");
  if (env && env[0] && !parse_int_arg("PLUMOS_SAFE_HOTKEY_TIMEOUT_MS", env, 0, 86400000,
                                      &cfg->timeout_ms)) {
    return 0;
  }
  env = getenv("PLUMOS_SAFE_HOTKEY_DEBOUNCE_MS");
  if (env && env[0] && !parse_int_arg("PLUMOS_SAFE_HOTKEY_DEBOUNCE_MS", env, 0, 60000,
                                      &cfg->debounce_ms)) {
    return 0;
  }
  env = getenv("PLUMOS_SAFE_HOTKEYD_VOLUME_KEYS");
  if (env && env[0]) {
    cfg->volume_keys_enabled =
        !(strcmp(env, "0") == 0 || strcmp(env, "false") == 0 ||
          strcmp(env, "False") == 0 || strcmp(env, "no") == 0 ||
          strcmp(env, "No") == 0 || strcmp(env, "off") == 0 ||
          strcmp(env, "Off") == 0);
  }
  return 1;
}

static void usage(const char *argv0) {
  printf("Usage: %s [options]\n", argv0);
  printf("Options:\n");
  printf("  --event PATH           Trigger input device. Default: auto by key code.\n");
  printf("  --root PATH            plumOS root. Default: %s.\n", DEFAULT_ROOT);
  printf("  --sdcard-root PATH     SD card root. Default: %s.\n", DEFAULT_SDCARD_ROOT);
  printf("  --log PATH             Daemon log path. Default: PLUMOS_ROOT/logs/safe-hotkeyd.log.\n");
  printf("  --action NAME          shutdown or sleep. Default: shutdown.\n");
  printf("  --shutdown             Same as --action shutdown.\n");
  printf("  --sleep                Same as --action sleep.\n");
  printf("  --poweroff             Pass --poweroff to the power action helper.\n");
  printf("  --no-poweroff          Pass --no-poweroff to the power action helper. Default.\n");
  printf("  --command COMMAND      Shell command to run on trigger.\n");
  printf("  --key-code N           EV_KEY code to watch. Default: %d (KEY_POWER).\n",
         KEY_POWER);
  printf("  --volume-event PATH    Event device for volume keys. Default: auto gpio-keys-polled.\n");
  printf("  --timeout-ms MS        Exit after MS without a trigger. Default: 0, run forever.\n");
  printf("  --debounce-ms MS       Ignore repeated triggers for MS. Default: %d.\n",
         DEFAULT_DEBOUNCE_MS);
  printf("  --volume-only          Handle volume keys without monitoring the power key.\n");
  printf("  --no-volume-keys       Do not handle KEY_VOLUMEUP/KEY_VOLUMEDOWN.\n");
  printf("  --oneshot              Exit after the first trigger command completes.\n");
  printf("  --dry-run              Log triggers without running the command.\n");
  printf("  --trigger-now          Run the trigger command immediately, then exit.\n");
  printf("  --verbose              Mirror log lines to stdout.\n");
  printf("  -h, --help             Show this help.\n");
  printf("\nSignals:\n");
  printf("  SIGUSR1                Trigger the same command path as the physical power key.\n");
}

static int parse_args(struct config *cfg, int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
      const char *value = argv[++i];
      if (strcmp(value, "auto") == 0) {
        if (!discover_input_event(cfg->event_path, sizeof(cfg->event_path), cfg->key_code)) {
          return 0;
        }
        cfg->event_path_explicit = 0;
      } else if (!copy_string(cfg->event_path, sizeof(cfg->event_path), value)) {
        return 0;
      } else {
        cfg->event_path_explicit = 1;
      }
    } else if (strcmp(argv[i], "--volume-event") == 0 && i + 1 < argc) {
      const char *value = argv[++i];
      if (strcmp(value, "auto") == 0) {
        if (!discover_volume_input_event(cfg->volume_event_path, sizeof(cfg->volume_event_path))) {
          return 0;
        }
        cfg->volume_event_path_explicit = 0;
      } else if (!copy_string(cfg->volume_event_path, sizeof(cfg->volume_event_path), value)) {
        return 0;
      } else {
        cfg->volume_event_path_explicit = 1;
      }
    } else if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
      if (!copy_string(cfg->root, sizeof(cfg->root), argv[++i])) {
        return 0;
      }
    } else if (strcmp(argv[i], "--sdcard-root") == 0 && i + 1 < argc) {
      if (!copy_string(cfg->sdcard_root, sizeof(cfg->sdcard_root), argv[++i])) {
        return 0;
      }
    } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
      if (!copy_string(cfg->log_path, sizeof(cfg->log_path), argv[++i])) {
        return 0;
      }
    } else if (strcmp(argv[i], "--action") == 0 && i + 1 < argc) {
      if (!parse_action(argv[++i], cfg->action, sizeof(cfg->action))) {
        return 0;
      }
    } else if (strcmp(argv[i], "--shutdown") == 0) {
      if (!parse_action("shutdown", cfg->action, sizeof(cfg->action))) {
        return 0;
      }
    } else if (strcmp(argv[i], "--sleep") == 0) {
      if (!parse_action("sleep", cfg->action, sizeof(cfg->action))) {
        return 0;
      }
    } else if (strcmp(argv[i], "--poweroff") == 0) {
      cfg->poweroff = 1;
    } else if (strcmp(argv[i], "--no-poweroff") == 0) {
      cfg->poweroff = 0;
    } else if (strcmp(argv[i], "--command") == 0 && i + 1 < argc) {
      if (!copy_string(cfg->command, sizeof(cfg->command), argv[++i])) {
        return 0;
      }
      cfg->command_set = 1;
    } else if (strcmp(argv[i], "--key-code") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--key-code", argv[++i], 0, 1024, &cfg->key_code)) {
        return 0;
      }
      if (!cfg->event_path_explicit &&
          !discover_input_event(cfg->event_path, sizeof(cfg->event_path), cfg->key_code)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--timeout-ms", argv[++i], 0, 86400000, &cfg->timeout_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--debounce-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--debounce-ms", argv[++i], 0, 60000, &cfg->debounce_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--volume-only") == 0) {
      cfg->safe_key_enabled = 0;
      cfg->volume_keys_enabled = 1;
    } else if (strcmp(argv[i], "--no-volume-keys") == 0) {
      cfg->volume_keys_enabled = 0;
    } else if (strcmp(argv[i], "--oneshot") == 0) {
      cfg->oneshot = 1;
    } else if (strcmp(argv[i], "--dry-run") == 0) {
      cfg->dry_run = 1;
    } else if (strcmp(argv[i], "--trigger-now") == 0) {
      cfg->trigger_now = 1;
    } else if (strcmp(argv[i], "--verbose") == 0) {
      cfg->verbose = 1;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      exit(0);
    } else {
      fprintf(stderr, "error: unknown or incomplete option: %s\n", argv[i]);
      usage(argv[0]);
      return 0;
    }
  }
  if (!cfg->log_path[0] &&
      !join_path(cfg->log_path, sizeof(cfg->log_path), cfg->root, "logs/safe-hotkeyd.log")) {
    return 0;
  }
  if (cfg->safe_key_enabled && !cfg->event_path[0] &&
      !discover_input_event(cfg->event_path, sizeof(cfg->event_path), cfg->key_code)) {
    return 0;
  }
  if (cfg->volume_keys_enabled && !cfg->volume_event_path[0] &&
      !discover_volume_input_event(cfg->volume_event_path, sizeof(cfg->volume_event_path))) {
    return 0;
  }
  if (cfg->safe_key_enabled && !cfg->command_set && !build_default_command(cfg)) {
    return 0;
  }
  if (!cfg->safe_key_enabled && !cfg->volume_keys_enabled) {
    fprintf(stderr, "error: both power key and volume keys are disabled\n");
    return 0;
  }
  return 1;
}

static int acquire_volume_lock(struct config *cfg) {
  char pid_buf[64];
  int fd;
  int n;

  if (!cfg || !cfg->volume_keys_enabled) {
    return 1;
  }
  fd = open(VOLUME_LOCK_PATH, O_CREAT | O_RDWR, 0644);
  if (fd < 0) {
    log_msg(cfg, "warning volume_lock open_failed path=%s errno=%d", VOLUME_LOCK_PATH,
            errno);
    cfg->volume_keys_enabled = 0;
    return 0;
  }
  if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
    log_msg(cfg, "warning volume_lock busy path=%s errno=%d", VOLUME_LOCK_PATH, errno);
    close(fd);
    cfg->volume_keys_enabled = 0;
    return 0;
  }
  if (ftruncate(fd, 0) == 0) {
    n = snprintf(pid_buf, sizeof(pid_buf), "%ld\n", (long)getpid());
    if (n > 0) {
      (void)write(fd, pid_buf, (size_t)n);
    }
  }
  cfg->volume_lock_fd = fd;
  return 1;
}

static void release_volume_lock(struct config *cfg) {
  if (!cfg || cfg->volume_lock_fd < 0) {
    return;
  }
  flock(cfg->volume_lock_fd, LOCK_UN);
  close(cfg->volume_lock_fd);
  cfg->volume_lock_fd = -1;
}

static int run_trigger_command(struct config *cfg, const char *source) {
  char overlay_pids[1024];
  char *old_overlay_pids = NULL;
  const char *old_env;
  int had_old_env;
  long long collect_start;
  long long collect_elapsed;
  long long start;
  long long elapsed;
  int rc;
  int status;

  emit_msg(cfg, "trigger source=%s action=%s dry_run=%d", source, cfg->action, cfg->dry_run);
  if (cfg->dry_run) {
    emit_msg(cfg, "result=plumos_safe_hotkeyd_trigger source=%s rc=0 dry_run=1", source);
    return 0;
  }

  collect_start = now_ms();
  collect_fb0_owner_pids(overlay_pids, sizeof(overlay_pids));
  collect_elapsed = now_ms() - collect_start;
  emit_msg(cfg, "overlay_pids=\"%s\" collect_ms=%lld", overlay_pids, collect_elapsed);
  old_env = getenv(POWER_OVERLAY_PIDS_ENV);
  had_old_env = old_env != NULL;
  if (had_old_env) {
    old_overlay_pids = strdup(old_env);
  }
  setenv(POWER_OVERLAY_PIDS_ENV, overlay_pids, 1);

  start = now_ms();
  rc = system(cfg->command);
  elapsed = now_ms() - start;
  if (had_old_env && old_overlay_pids) {
    setenv(POWER_OVERLAY_PIDS_ENV, old_overlay_pids, 1);
    free(old_overlay_pids);
  } else {
    unsetenv(POWER_OVERLAY_PIDS_ENV);
  }
  if (rc == -1) {
    emit_msg(cfg, "result=plumos_safe_hotkeyd_trigger source=%s rc=-1 errno=%d elapsed_ms=%lld",
             source, errno, elapsed);
    return 1;
  }
  if (WIFEXITED(rc)) {
    status = WEXITSTATUS(rc);
  } else if (WIFSIGNALED(rc)) {
    status = 128 + WTERMSIG(rc);
  } else {
    status = 1;
  }
  emit_msg(cfg, "result=plumos_safe_hotkeyd_trigger source=%s rc=%d elapsed_ms=%lld", source,
           status, elapsed);
  return status == 0 ? 0 : 1;
}

static int run_volume_key_command(struct config *cfg, int direction) {
  char bin[PATH_MAX];
  char command_log[PATH_MAX];
  char q_root[SHELL_QUOTE_MAX];
  char q_bin[SHELL_QUOTE_MAX];
  char q_command_log[SHELL_QUOTE_MAX];
  char command[COMMAND_MAX];
  const char *action = direction > 0 ? "runtime-up" : "runtime-down";
  const char *source = direction > 0 ? "volume_up" : "volume_down";
  long long start;
  long long elapsed;
  int rc;
  int status;
  int n;

  emit_msg(cfg, "trigger source=%s action=volume dry_run=%d", source, cfg->dry_run);
  if (cfg->dry_run) {
    emit_msg(cfg, "result=plumos_safe_hotkeyd_volume source=%s rc=0 dry_run=1", source);
    return 0;
  }
  if (!join_path(bin, sizeof(bin), cfg->root, "bin/plumos-volume-control") ||
      !join_path(command_log, sizeof(command_log), cfg->root,
                 "logs/safe-hotkeyd-command.log")) {
    emit_msg(cfg, "result=plumos_safe_hotkeyd_volume source=%s rc=1 reason=path", source);
    return 1;
  }
  if (!shell_quote(q_root, sizeof(q_root), cfg->root) ||
      !shell_quote(q_bin, sizeof(q_bin), bin) ||
      !shell_quote(q_command_log, sizeof(q_command_log), command_log)) {
    emit_msg(cfg, "result=plumos_safe_hotkeyd_volume source=%s rc=1 reason=quote", source);
    return 1;
  }

  n = snprintf(command, sizeof(command), "PLUMOS_ROOT=%s %s %s >> %s 2>&1",
               q_root, q_bin, action, q_command_log);
  if (n < 0 || (size_t)n >= sizeof(command)) {
    emit_msg(cfg, "result=plumos_safe_hotkeyd_volume source=%s rc=1 reason=command_length",
             source);
    return 1;
  }

  start = now_ms();
  rc = system(command);
  elapsed = now_ms() - start;
  if (rc == -1) {
    emit_msg(cfg, "result=plumos_safe_hotkeyd_volume source=%s rc=-1 errno=%d elapsed_ms=%lld",
             source, errno, elapsed);
    return 1;
  }
  if (WIFEXITED(rc)) {
    status = WEXITSTATUS(rc);
  } else if (WIFSIGNALED(rc)) {
    status = 128 + WTERMSIG(rc);
  } else {
    status = 1;
  }
  emit_msg(cfg, "result=plumos_safe_hotkeyd_volume source=%s rc=%d elapsed_ms=%lld",
           source, status, elapsed);
  return status == 0 ? 0 : 1;
}

static int handle_trigger(struct config *cfg, const char *source, long long *last_trigger_ms) {
  long long now;
  int rc;

  now = now_ms();
  if (*last_trigger_ms > 0 && cfg->debounce_ms > 0 &&
      now - *last_trigger_ms < cfg->debounce_ms) {
    log_msg(cfg, "ignored trigger source=%s reason=debounce elapsed_ms=%lld", source,
            now - *last_trigger_ms);
    return 0;
  }
  *last_trigger_ms = now;
  rc = run_trigger_command(cfg, source);
  *last_trigger_ms = now_ms();
  return rc;
}

static int drain_event_fd(struct config *cfg, int fd, const char *source, long long *last_trigger_ms,
                          int *exit_rc) {
  for (;;) {
    struct input_event ev;
    ssize_t n = read(fd, &ev, sizeof(ev));
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        break;
      }
      emit_msg(cfg, "error read source=%s errno=%d", source, errno);
      *exit_rc = 1;
      return 0;
    }
    if (n == 0) {
      break;
    }
    if ((size_t)n != sizeof(ev)) {
      log_msg(cfg, "ignored short_read source=%s bytes=%ld", source, (long)n);
      continue;
    }
    if (ev.type == EV_KEY && cfg->volume_keys_enabled &&
        (ev.code == KEY_VOLUMEDOWN || ev.code == KEY_VOLUMEUP) &&
        (ev.value == 1 || ev.value == 2)) {
      run_volume_key_command(cfg, ev.code == KEY_VOLUMEUP ? 1 : -1);
      continue;
    }
    if (cfg->safe_key_enabled && ev.type == EV_KEY && ev.code == cfg->key_code &&
        ev.value == 1) {
      *exit_rc = handle_trigger(cfg, "key", last_trigger_ms);
      if (cfg->oneshot) {
        g_stop = 1;
        return 0;
      }
    }
  }
  return 1;
}

static int event_loop(struct config *cfg) {
  int fd = -1;
  int volume_fd = -1;
  long long start;
  long long deadline = 0;
  long long last_trigger_ms = 0;
  int exit_rc = 0;

  if (cfg->volume_keys_enabled) {
    acquire_volume_lock(cfg);
  }

  if (cfg->safe_key_enabled) {
    fd = open(cfg->event_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
      fprintf(stderr, "error: cannot open input event device: %s: %s\n", cfg->event_path,
              strerror(errno));
      log_msg(cfg, "error open_event path=%s errno=%d", cfg->event_path, errno);
      release_volume_lock(cfg);
      return 1;
    }
  }
  if (cfg->volume_keys_enabled && cfg->volume_event_path[0] && !same_path(cfg->event_path, cfg->volume_event_path)) {
    volume_fd = open(cfg->volume_event_path, O_RDONLY | O_NONBLOCK);
    if (volume_fd < 0) {
      log_msg(cfg, "warning open_volume_event path=%s errno=%d", cfg->volume_event_path, errno);
    }
  }
  if (fd < 0 && volume_fd < 0) {
    fprintf(stderr, "error: no input event device is available\n");
    log_msg(cfg, "error no_input_event safe_key=%d volume_keys=%d", cfg->safe_key_enabled,
            cfg->volume_keys_enabled);
    release_volume_lock(cfg);
    return 1;
  }

  start = now_ms();
  if (cfg->timeout_ms > 0) {
    deadline = start + cfg->timeout_ms;
  }
  emit_msg(cfg,
           "plumos-safe-hotkeyd event=%s key_code=%d volume_event=%s action=%s timeout_ms=%d "
           "oneshot=%d safe_key=%d volume_keys=%d",
           cfg->safe_key_enabled ? cfg->event_path : "off", cfg->key_code,
           volume_fd >= 0 ? cfg->volume_event_path : cfg->event_path, cfg->action,
           cfg->timeout_ms, cfg->oneshot, cfg->safe_key_enabled, cfg->volume_keys_enabled);

  while (!g_stop) {
    struct pollfd pfds[2];
    int trigger_index = -1;
    int volume_index = -1;
    nfds_t nfds = 0;
    int poll_ms = DEFAULT_POLL_MS;
    int prc;

    if (g_trigger_signal) {
      g_trigger_signal = 0;
      if (cfg->safe_key_enabled) {
        exit_rc = handle_trigger(cfg, "signal", &last_trigger_ms);
        if (cfg->oneshot) {
          break;
        }
      } else {
        log_msg(cfg, "ignored trigger source=signal reason=volume_only");
      }
    }

    if (deadline > 0) {
      long long remaining = deadline - now_ms();
      if (remaining <= 0) {
        log_msg(cfg, "exit reason=timeout");
        break;
      }
      if (remaining < poll_ms) {
        poll_ms = (int)remaining;
      }
    }

    if (fd >= 0) {
      trigger_index = (int)nfds;
      pfds[nfds].fd = fd;
      pfds[nfds].events = POLLIN;
      pfds[nfds].revents = 0;
      nfds++;
    }
    if (volume_fd >= 0) {
      volume_index = (int)nfds;
      pfds[nfds].fd = volume_fd;
      pfds[nfds].events = POLLIN;
      pfds[nfds].revents = 0;
      nfds++;
    }

    prc = poll(pfds, nfds, poll_ms);
    if (prc < 0) {
      if (errno == EINTR) {
        continue;
      }
      emit_msg(cfg, "error poll errno=%d", errno);
      exit_rc = 1;
      break;
    }
    if (prc == 0) {
      continue;
    }

    if (trigger_index >= 0 && (pfds[trigger_index].revents & POLLIN) &&
        !drain_event_fd(cfg, fd, "trigger", &last_trigger_ms, &exit_rc)) {
      break;
    }
    if (volume_index >= 0 && (pfds[volume_index].revents & POLLIN) &&
        !drain_event_fd(cfg, volume_fd, "volume", &last_trigger_ms, &exit_rc)) {
      break;
    }
    if (exit_rc != 0 && cfg->oneshot) {
      break;
    }
  }

  if (fd >= 0) {
    close(fd);
  }
  if (volume_fd >= 0) {
    close(volume_fd);
  }
  release_volume_lock(cfg);
  log_msg(cfg, "exit rc=%d stop=%d", exit_rc, (int)g_stop);
  return exit_rc;
}

int main(int argc, char **argv) {
  struct config cfg;
  long long last_trigger_ms = 0;
  struct sigaction sa;

  if (!init_config(&cfg) || !parse_args(&cfg, argc, argv)) {
    return 2;
  }
  ensure_parent_dir(cfg.log_path);

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGUSR1, &sa, NULL);

  if (cfg.trigger_now) {
    if (!cfg.safe_key_enabled) {
      fprintf(stderr, "error: --trigger-now cannot be used with --volume-only\n");
      return 2;
    }
    return handle_trigger(&cfg, "manual", &last_trigger_ms);
  }
  return event_loop(&cfg);
}
