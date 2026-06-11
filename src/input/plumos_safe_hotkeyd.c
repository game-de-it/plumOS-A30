#include <ctype.h>
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

#define DEFAULT_ROOT "/mnt/SDCARD/plumos"
#define DEFAULT_SDCARD_ROOT "/mnt/SDCARD"
#define DEFAULT_DEBOUNCE_MS 1500
#define DEFAULT_POLL_MS 250
#define COMMAND_MAX 8192
#define SHELL_QUOTE_MAX (PATH_MAX * 4 + 4)

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_trigger_signal = 0;

struct config {
  char root[PATH_MAX];
  char sdcard_root[PATH_MAX];
  char event_path[PATH_MAX];
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
  int volume_keys_enabled;
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

static int discover_input_event(char *out, size_t out_size) {
  FILE *f;
  char line[512];
  int in_gpio = 0;

  f = fopen("/proc/bus/input/devices", "rb");
  if (!f) {
    return copy_string(out, out_size, "/dev/input/event3");
  }
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '\n') {
      in_gpio = 0;
      continue;
    }
    if (strstr(line, "Name=\"gpio-keys-polled\"")) {
      in_gpio = 1;
      continue;
    }
    if (in_gpio && strstr(line, "Handlers=")) {
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
  return copy_string(out, out_size, "/dev/input/event3");
}

static int build_default_command(struct config *cfg) {
  char bin[PATH_MAX];
  char command_log[PATH_MAX];
  char q_root[SHELL_QUOTE_MAX];
  char q_sdcard[SHELL_QUOTE_MAX];
  char q_bin[SHELL_QUOTE_MAX];
  char q_command_log[SHELL_QUOTE_MAX];
  const char *power_arg;
  int n;

  if (!join_path(bin, sizeof(bin), cfg->root, "bin/plumos-safe-shutdown") ||
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

  power_arg = cfg->poweroff ? "--poweroff" : "--no-poweroff";
  n = snprintf(cfg->command, sizeof(cfg->command),
               "PLUMOS_ROOT=%s PLUMOS_SDCARD_ROOT=%s %s --%s %s >> %s 2>&1", q_root,
               q_sdcard, q_bin, cfg->action, power_arg, q_command_log);
  if (n < 0 || (size_t)n >= sizeof(cfg->command)) {
    fprintf(stderr, "error: safe shutdown command is too long\n");
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
  cfg->key_code = KEY_ESC;
  cfg->debounce_ms = DEFAULT_DEBOUNCE_MS;
  cfg->volume_keys_enabled = 1;

  env = getenv("PLUMOS_ROOT");
  if (env && env[0] && !copy_string(cfg->root, sizeof(cfg->root), env)) {
    return 0;
  }
  env = getenv("PLUMOS_SDCARD_ROOT");
  if (env && env[0] && !copy_string(cfg->sdcard_root, sizeof(cfg->sdcard_root), env)) {
    return 0;
  }
  env = getenv("PLUMOS_SAFE_HOTKEY_EVENT");
  if (env && env[0]) {
    if (!copy_string(cfg->event_path, sizeof(cfg->event_path), env)) {
      return 0;
    }
  } else if (!discover_input_event(cfg->event_path, sizeof(cfg->event_path))) {
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
  env = getenv("PLUMOS_SAFE_HOTKEY_KEY_CODE");
  if (env && env[0] && !parse_int_arg("PLUMOS_SAFE_HOTKEY_KEY_CODE", env, 0, 1024,
                                      &cfg->key_code)) {
    return 0;
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
  printf("  --event PATH           Raw input event device. Default: auto gpio-keys-polled.\n");
  printf("  --root PATH            plumOS root. Default: %s.\n", DEFAULT_ROOT);
  printf("  --sdcard-root PATH     SD card root. Default: %s.\n", DEFAULT_SDCARD_ROOT);
  printf("  --log PATH             Daemon log path. Default: PLUMOS_ROOT/logs/safe-hotkeyd.log.\n");
  printf("  --action NAME          shutdown or sleep. Default: shutdown.\n");
  printf("  --shutdown             Same as --action shutdown.\n");
  printf("  --sleep                Same as --action sleep.\n");
  printf("  --poweroff             Pass --poweroff to plumos-safe-shutdown.\n");
  printf("  --no-poweroff          Pass --no-poweroff to plumos-safe-shutdown. Default.\n");
  printf("  --command COMMAND      Shell command to run on trigger.\n");
  printf("  --key-code N           EV_KEY code to watch. Default: %d (KEY_ESC/Function).\n",
         KEY_ESC);
  printf("  --timeout-ms MS        Exit after MS without a trigger. Default: 0, run forever.\n");
  printf("  --debounce-ms MS       Ignore repeated triggers for MS. Default: %d.\n",
         DEFAULT_DEBOUNCE_MS);
  printf("  --no-volume-keys       Do not handle KEY_VOLUMEUP/KEY_VOLUMEDOWN.\n");
  printf("  --oneshot              Exit after the first trigger command completes.\n");
  printf("  --dry-run              Log triggers without running the command.\n");
  printf("  --trigger-now          Run the trigger command immediately, then exit.\n");
  printf("  --verbose              Mirror log lines to stdout.\n");
  printf("  -h, --help             Show this help.\n");
  printf("\nSignals:\n");
  printf("  SIGUSR1                Trigger the same command path as the Function key.\n");
}

static int parse_args(struct config *cfg, int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
      const char *value = argv[++i];
      if (strcmp(value, "auto") == 0) {
        if (!discover_input_event(cfg->event_path, sizeof(cfg->event_path))) {
          return 0;
        }
      } else if (!copy_string(cfg->event_path, sizeof(cfg->event_path), value)) {
        return 0;
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
    } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--timeout-ms", argv[++i], 0, 86400000, &cfg->timeout_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--debounce-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--debounce-ms", argv[++i], 0, 60000, &cfg->debounce_ms)) {
        return 0;
      }
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
  if (!cfg->command_set && !build_default_command(cfg)) {
    return 0;
  }
  return 1;
}

static int run_trigger_command(struct config *cfg, const char *source) {
  long long start;
  long long elapsed;
  int rc;
  int status;

  emit_msg(cfg, "trigger source=%s action=%s dry_run=%d", source, cfg->action, cfg->dry_run);
  if (cfg->dry_run) {
    emit_msg(cfg, "result=plumos_safe_hotkeyd_trigger source=%s rc=0 dry_run=1", source);
    return 0;
  }

  start = now_ms();
  rc = system(cfg->command);
  elapsed = now_ms() - start;
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
  const char *action = direction > 0 ? "up" : "down";
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

  now = now_ms();
  if (*last_trigger_ms > 0 && cfg->debounce_ms > 0 &&
      now - *last_trigger_ms < cfg->debounce_ms) {
    log_msg(cfg, "ignored trigger source=%s reason=debounce elapsed_ms=%lld", source,
            now - *last_trigger_ms);
    return 0;
  }
  *last_trigger_ms = now;
  return run_trigger_command(cfg, source);
}

static int event_loop(struct config *cfg) {
  int fd;
  long long start;
  long long deadline = 0;
  long long last_trigger_ms = 0;
  int exit_rc = 0;

  fd = open(cfg->event_path, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    fprintf(stderr, "error: cannot open input event device: %s: %s\n", cfg->event_path,
            strerror(errno));
    log_msg(cfg, "error open_event path=%s errno=%d", cfg->event_path, errno);
    return 1;
  }

  start = now_ms();
  if (cfg->timeout_ms > 0) {
    deadline = start + cfg->timeout_ms;
  }
  emit_msg(cfg,
           "plumos-safe-hotkeyd event=%s key_code=%d action=%s timeout_ms=%d oneshot=%d "
           "volume_keys=%d",
           cfg->event_path, cfg->key_code, cfg->action, cfg->timeout_ms, cfg->oneshot,
           cfg->volume_keys_enabled);

  while (!g_stop) {
    struct pollfd pfd;
    int poll_ms = DEFAULT_POLL_MS;
    int prc;

    if (g_trigger_signal) {
      g_trigger_signal = 0;
      exit_rc = handle_trigger(cfg, "signal", &last_trigger_ms);
      if (cfg->oneshot) {
        break;
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

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    prc = poll(&pfd, 1, poll_ms);
    if (prc < 0) {
      if (errno == EINTR) {
        continue;
      }
      emit_msg(cfg, "error poll errno=%d", errno);
      exit_rc = 1;
      break;
    }
    if (prc == 0 || !(pfd.revents & POLLIN)) {
      continue;
    }

    for (;;) {
      struct input_event ev;
      ssize_t n = read(fd, &ev, sizeof(ev));
      if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
          break;
        }
        emit_msg(cfg, "error read errno=%d", errno);
        exit_rc = 1;
        break;
      }
      if (n == 0) {
        break;
      }
      if ((size_t)n != sizeof(ev)) {
        log_msg(cfg, "ignored short_read bytes=%ld", (long)n);
        continue;
      }
      if (ev.type == EV_KEY && cfg->volume_keys_enabled &&
          (ev.code == KEY_VOLUMEDOWN || ev.code == KEY_VOLUMEUP) &&
          (ev.value == 1 || ev.value == 2)) {
        run_volume_key_command(cfg, ev.code == KEY_VOLUMEUP ? 1 : -1);
        continue;
      }
      if (ev.type == EV_KEY && ev.code == cfg->key_code && ev.value == 1) {
        exit_rc = handle_trigger(cfg, "key", &last_trigger_ms);
        if (cfg->oneshot) {
          g_stop = 1;
          break;
        }
      }
    }
    if (exit_rc != 0 && cfg->oneshot) {
      break;
    }
  }

  close(fd);
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
    return handle_trigger(&cfg, "manual", &last_trigger_ms);
  }
  return event_loop(&cfg);
}
