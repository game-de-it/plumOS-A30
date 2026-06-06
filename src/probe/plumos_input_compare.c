#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<linux/input.h>)
#include <linux/input.h>
#define PLUMOS_HAS_LINUX_INPUT 1
#endif
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef PLUMOS_HAS_LINUX_INPUT
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
#ifndef KEY_BACKSPACE
#define KEY_BACKSPACE 14
#endif
#ifndef KEY_TAB
#define KEY_TAB 15
#endif
#ifndef KEY_Q
#define KEY_Q 16
#endif
#ifndef KEY_Z
#define KEY_Z 44
#endif
#ifndef KEY_X
#define KEY_X 45
#endif
#ifndef KEY_SPACE
#define KEY_SPACE 57
#endif
#ifndef KEY_HOME
#define KEY_HOME 102
#endif
#ifndef KEY_UP
#define KEY_UP 103
#endif
#ifndef KEY_LEFT
#define KEY_LEFT 105
#endif
#ifndef KEY_RIGHT
#define KEY_RIGHT 106
#endif
#ifndef KEY_DOWN
#define KEY_DOWN 108
#endif
#ifndef KEY_RIGHTCTRL
#define KEY_RIGHTCTRL 97
#endif
#ifndef KEY_ENTER
#define KEY_ENTER 28
#endif
#ifndef BTN_SOUTH
#define BTN_SOUTH 304
#endif
#ifndef BTN_EAST
#define BTN_EAST 305
#endif
#ifndef BTN_SELECT
#define BTN_SELECT 314
#endif
#ifndef BTN_START
#define BTN_START 315
#endif
#ifndef KEY_SELECT
#define KEY_SELECT 0x161
#endif
#ifndef KEY_MENU
#define KEY_MENU 139
#endif

#define MAX_INPUT_DEVICES 16
#define MAX_PROC_REFS 64

struct input_device_info {
  char name[128];
  char handlers[256];
  char event_name[32];
  char path[PATH_MAX];
  int is_gpio;
};

struct proc_ref {
  int pid;
  char comm[64];
  char fd_name[32];
  char target[PATH_MAX];
};

struct compare_state {
  struct input_device_info devices[MAX_INPUT_DEVICES];
  struct proc_ref refs[MAX_PROC_REFS];
  int device_count;
  int ref_count;
  int keymon_running;
  int mainui_running;
  int keymon_holds_direct;
  int mainui_holds_direct;
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
    memcpy(out, in, out_size - 1);
    out[out_size - 1] = '\0';
    return 0;
  }
  memcpy(out, in, len + 1);
  return 1;
}

static void chomp(char *s) {
  size_t len;
  if (!s) {
    return;
  }
  len = strlen(s);
  while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
    s[--len] = '\0';
  }
}

static int is_decimal(const char *s) {
  if (!s || !s[0]) {
    return 0;
  }
  while (*s) {
    if (!isdigit((unsigned char)*s++)) {
      return 0;
    }
  }
  return 1;
}

static int join_path(char *out, size_t out_size, const char *a, const char *b) {
  size_t len_a;
  size_t len_b;

  if (!out || out_size == 0 || !a || !b) {
    return 0;
  }
  len_a = strlen(a);
  len_b = strlen(b);
  if (len_a + 1 + len_b + 1 > out_size) {
    return 0;
  }
  memcpy(out, a, len_a);
  out[len_a] = '/';
  memcpy(out + len_a + 1, b, len_b + 1);
  return 1;
}

static int parse_int_arg(const char *name, const char *value, int min_value, int max_value,
                         int *out) {
  char *end = NULL;
  long n;

  errno = 0;
  n = strtol(value, &end, 10);
  if (errno || !end || *end || n < min_value || n > max_value) {
    fprintf(stderr, "error: invalid %s: %s\n", name, value);
    return 0;
  }
  *out = (int)n;
  return 1;
}

static long long now_ms(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    return 0;
  }
  return (long long)tv.tv_sec * 1000LL + (long long)tv.tv_usec / 1000LL;
}

static const char *event_action(unsigned short code) {
  switch (code) {
  case KEY_UP:
    return "up";
  case KEY_DOWN:
    return "down";
  case KEY_LEFT:
    return "left";
  case KEY_RIGHT:
    return "right";
  case KEY_ENTER:
  case KEY_SPACE:
  case BTN_SOUTH:
  case KEY_Z:
    return "a";
  case KEY_ESC:
  case KEY_BACKSPACE:
  case BTN_EAST:
  case KEY_X:
    return "b";
  case KEY_MENU:
  case BTN_START:
  case KEY_RIGHTCTRL:
  case KEY_HOME:
    return "start";
  case KEY_SELECT:
  case BTN_SELECT:
  case KEY_TAB:
    return "select";
  case KEY_Q:
    return "quit";
  default:
    return "-";
  }
}

static void extract_quoted_name(const char *line, char *out, size_t out_size) {
  const char *start = strchr(line, '"');
  const char *end;
  size_t len;

  if (!start) {
    copy_string(out, out_size, line);
    return;
  }
  start++;
  end = strchr(start, '"');
  if (!end) {
    copy_string(out, out_size, start);
    return;
  }
  len = (size_t)(end - start);
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, start, len);
  out[len] = '\0';
}

static void extract_event_name(const char *handlers, char *out, size_t out_size) {
  const char *event = strstr(handlers, "event");
  size_t n = 0;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!event) {
    return;
  }
  while (event[n] && !isspace((unsigned char)event[n]) && n + 1 < out_size) {
    out[n] = event[n];
    n++;
  }
  out[n] = '\0';
}

static void store_input_device(struct compare_state *state,
                               const struct input_device_info *current) {
  struct input_device_info *device;
  char event_name[32];

  if (!current->event_name[0] || state->device_count >= MAX_INPUT_DEVICES) {
    return;
  }
  copy_string(event_name, sizeof(event_name), current->event_name);
  device = &state->devices[state->device_count++];
  *device = *current;
  snprintf(device->path, sizeof(device->path), "/dev/input/%s", event_name);
  device->is_gpio = strcmp(device->name, "gpio-keys-polled") == 0;
}

static void load_input_devices(struct compare_state *state, char *gpio_path,
                               size_t gpio_path_size) {
  FILE *f;
  char line[512];
  struct input_device_info current;

  if (gpio_path && gpio_path_size > 0) {
    gpio_path[0] = '\0';
  }
  memset(&current, 0, sizeof(current));
  f = fopen("/proc/bus/input/devices", "rb");
  if (!f) {
    copy_string(gpio_path, gpio_path_size, "/dev/input/event3");
    return;
  }
  while (fgets(line, sizeof(line), f)) {
    chomp(line);
    if (!line[0]) {
      store_input_device(state, &current);
      memset(&current, 0, sizeof(current));
      continue;
    }
    if (strncmp(line, "N: Name=", 8) == 0) {
      extract_quoted_name(line + 8, current.name, sizeof(current.name));
    } else if (strncmp(line, "H: Handlers=", 12) == 0) {
      copy_string(current.handlers, sizeof(current.handlers), line + 12);
      extract_event_name(current.handlers, current.event_name, sizeof(current.event_name));
    }
  }
  store_input_device(state, &current);
  fclose(f);

  for (int i = 0; i < state->device_count; i++) {
    if (state->devices[i].is_gpio) {
      copy_string(gpio_path, gpio_path_size, state->devices[i].path);
      return;
    }
  }
  copy_string(gpio_path, gpio_path_size, "/dev/input/event3");
}

static int read_comm(int pid, char *out, size_t out_size) {
  char path[PATH_MAX];
  FILE *f;

  snprintf(path, sizeof(path), "/proc/%d/comm", pid);
  f = fopen(path, "rb");
  if (!f) {
    return 0;
  }
  if (!fgets(out, (int)out_size, f)) {
    fclose(f);
    return 0;
  }
  fclose(f);
  chomp(out);
  return 1;
}

static void scan_process_fds(struct compare_state *state, int pid, const char *comm) {
  char fd_dir_path[PATH_MAX];
  DIR *fd_dir;
  struct dirent *entry;

  snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/%d/fd", pid);
  fd_dir = opendir(fd_dir_path);
  if (!fd_dir) {
    return;
  }
  while ((entry = readdir(fd_dir)) != NULL) {
    char link_path[PATH_MAX];
    char target[PATH_MAX];
    ssize_t n;
    struct proc_ref *ref;

    if (entry->d_name[0] == '.') {
      continue;
    }
    if (!join_path(link_path, sizeof(link_path), fd_dir_path, entry->d_name)) {
      continue;
    }
    n = readlink(link_path, target, sizeof(target) - 1);
    if (n < 0) {
      continue;
    }
    target[n] = '\0';
    if (strncmp(target, "/dev/input/", 11) != 0 && strcmp(target, "/dev/uinput") != 0) {
      continue;
    }
    if (state->ref_count >= MAX_PROC_REFS) {
      break;
    }
    ref = &state->refs[state->ref_count++];
    memset(ref, 0, sizeof(*ref));
    ref->pid = pid;
    copy_string(ref->comm, sizeof(ref->comm), comm);
    copy_string(ref->fd_name, sizeof(ref->fd_name), entry->d_name);
    copy_string(ref->target, sizeof(ref->target), target);
  }
  closedir(fd_dir);
}

static void load_process_refs(struct compare_state *state) {
  DIR *proc;
  struct dirent *entry;

  proc = opendir("/proc");
  if (!proc) {
    return;
  }
  while ((entry = readdir(proc)) != NULL) {
    int pid;
    char comm[64];

    if (!is_decimal(entry->d_name)) {
      continue;
    }
    pid = atoi(entry->d_name);
    if (!read_comm(pid, comm, sizeof(comm))) {
      continue;
    }
    if (strcmp(comm, "keymon") == 0) {
      state->keymon_running = 1;
      scan_process_fds(state, pid, comm);
    } else if (strcmp(comm, "MainUI") == 0) {
      state->mainui_running = 1;
      scan_process_fds(state, pid, comm);
    }
  }
  closedir(proc);
}

static int poll_direct_input(const char *event_path, int timeout_ms, int *events_out,
                             int *key_events_out) {
  int fd;
  long long deadline;
  int events = 0;
  int key_events = 0;

  fd = open(event_path, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    printf("direct path=%s open=no errno=%d %s\n", event_path, errno, strerror(errno));
    return 0;
  }
  deadline = now_ms() + timeout_ms;
  while (now_ms() < deadline) {
    struct pollfd pfd;
    int rc;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    rc = poll(&pfd, 1, 50);
    if (rc > 0 && (pfd.revents & POLLIN)) {
      struct input_event ev;
      while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        events++;
        if (ev.type == EV_KEY) {
          key_events++;
          printf("direct event type=%u code=%u value=%d action=%s\n", ev.type, ev.code,
                 ev.value, event_action(ev.code));
        }
      }
    }
  }
  close(fd);
  printf("direct path=%s open=yes timeout_ms=%d events=%d key_events=%d\n",
         event_path, timeout_ms, events, key_events);
  if (events_out) {
    *events_out = events;
  }
  if (key_events_out) {
    *key_events_out = key_events;
  }
  return 1;
}

static void update_hold_flags(struct compare_state *state, const char *event_path) {
  for (int i = 0; i < state->ref_count; i++) {
    struct proc_ref *ref = &state->refs[i];
    if (strcmp(ref->target, event_path) != 0) {
      continue;
    }
    if (strcmp(ref->comm, "keymon") == 0) {
      state->keymon_holds_direct = 1;
    } else if (strcmp(ref->comm, "MainUI") == 0) {
      state->mainui_holds_direct = 1;
    }
  }
}

static void print_report(const struct compare_state *state, const char *event_path,
                         int direct_open) {
  const char *direct_open_label = direct_open < 0 ? "skipped" : direct_open ? "yes" : "no";

  printf("input devices=%d selected=%s\n", state->device_count, event_path);
  for (int i = 0; i < state->device_count; i++) {
    const struct input_device_info *device = &state->devices[i];
    printf("device path=%s name=%s handlers=%s gpio=%s\n", device->path,
           device->name[0] ? device->name : "-", device->handlers[0] ? device->handlers : "-",
           device->is_gpio ? "yes" : "no");
  }
  printf("process keymon_running=%s mainui_running=%s refs=%d\n",
         state->keymon_running ? "yes" : "no", state->mainui_running ? "yes" : "no",
         state->ref_count);
  for (int i = 0; i < state->ref_count; i++) {
    const struct proc_ref *ref = &state->refs[i];
    printf("process_ref pid=%d comm=%s fd=%s target=%s\n", ref->pid, ref->comm,
           ref->fd_name, ref->target);
  }
  printf("comparison keymon_holds_selected=%s mainui_holds_selected=%s direct_open=%s\n",
         state->keymon_holds_direct ? "yes" : "no",
         state->mainui_holds_direct ? "yes" : "no", direct_open_label);
  if (direct_open < 0) {
    printf("decision=inspect_only\n");
  } else if (direct_open && state->keymon_running) {
    printf("decision=keep_keymon_for_now; direct_input_is_viable_nonexclusive\n");
  } else if (direct_open) {
    printf("decision=direct_input_is_viable; keymon_not_running\n");
  } else {
    printf("decision=direct_input_blocked; keep_keymon_or_recheck_permissions\n");
  }
}

static void usage(const char *argv0) {
  printf("Usage: %s [OPTIONS]\n", argv0);
  printf("\n");
  printf("Options:\n");
  printf("  --event PATH       Input event path. Default: gpio-keys-polled auto-detect\n");
  printf("  --timeout-ms MS    Direct input poll duration. Default: 250\n");
  printf("  --no-poll          Only inspect devices/process fds\n");
}

int main(int argc, char **argv) {
  struct compare_state state;
  char event_path[PATH_MAX];
  int timeout_ms = 250;
  int no_poll = 0;
  int direct_open = 0;
  int events = 0;
  int key_events = 0;

  memset(&state, 0, sizeof(state));
  load_input_devices(&state, event_path, sizeof(event_path));

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
      copy_string(event_path, sizeof(event_path), argv[++i]);
    } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--timeout-ms", argv[++i], 0, 60000, &timeout_ms)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--no-poll") == 0) {
      no_poll = 1;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "error: unknown or incomplete option: %s\n", argv[i]);
      usage(argv[0]);
      return 2;
    }
  }

  load_process_refs(&state);
  update_hold_flags(&state, event_path);

  printf("plumOS input compare\n");
  if (!no_poll) {
    direct_open = poll_direct_input(event_path, timeout_ms, &events, &key_events);
  } else {
    printf("direct path=%s poll=skipped\n", event_path);
    direct_open = -1;
  }
  (void)events;
  (void)key_events;
  print_report(&state, event_path, direct_open);
  return (!no_poll && !direct_open) ? 1 : 0;
}
