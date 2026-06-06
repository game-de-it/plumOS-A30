#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<linux/input.h>)
#include <linux/input.h>
#define PLUMOS_HAS_LINUX_INPUT 1
#endif
#if __has_include(<linux/joystick.h>)
#include <linux/joystick.h>
#define PLUMOS_HAS_LINUX_JOYSTICK 1
#endif
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef PLUMOS_HAS_LINUX_INPUT
#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_ABS 0x03
#define SYN_REPORT 0
#define ABS_X 0
#define ABS_Y 1
#define ABS_Z 2
#define ABS_RX 3
#define ABS_RY 4
#define ABS_RZ 5
#define ABS_HAT0X 16
#define ABS_HAT0Y 17
#define BTN_A 304
#define BTN_B 305
#define BTN_X 307
#define BTN_Y 308
#define BTN_TL 310
#define BTN_TR 311
#define BTN_SELECT 314
#define BTN_START 315
#define BTN_MODE 316
#define BTN_THUMBL 317
#define BTN_THUMBR 318
struct input_event {
  struct timeval time;
  unsigned short type;
  unsigned short code;
  int value;
};
#endif

#ifndef PLUMOS_HAS_LINUX_JOYSTICK
#define JS_EVENT_BUTTON 0x01
#define JS_EVENT_AXIS 0x02
#define JS_EVENT_INIT 0x80
struct js_event {
  unsigned int time;
  short value;
  unsigned char type;
  unsigned char number;
};
#endif

#define DEFAULT_JS_PATH "/dev/input/js0"
#define DEFAULT_EVENT_PATH "/dev/input/event4"
#define DEFAULT_DEVICE_NAME "plumOS A30 Analog Stick"
#define DEFAULT_TIMEOUT_MS 3000

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

static int discover_event_by_name(const char *device_name, char *out, size_t out_size) {
  FILE *f;
  char line[512];
  int in_target = 0;

  f = fopen("/proc/bus/input/devices", "rb");
  if (!f) {
    return 0;
  }
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '\n') {
      in_target = 0;
      continue;
    }
    if (strstr(line, "Name=\"") && strstr(line, device_name)) {
      in_target = 1;
      continue;
    }
    if (in_target && strstr(line, "Handlers=")) {
      char *event = strstr(line, "event");
      if (event) {
        char name[32];
        size_t n = 0;
        while (event[n] && event[n] != ' ' && event[n] != '\n' && n + 1 < sizeof(name)) {
          name[n] = event[n];
          n++;
        }
        name[n] = '\0';
        fclose(f);
        snprintf(out, out_size, "/dev/input/%s", name);
        return 1;
      }
    }
  }
  fclose(f);
  return 0;
}

static const char *event_type_name(unsigned short type) {
  switch (type) {
  case EV_SYN:
    return "EV_SYN";
  case EV_KEY:
    return "EV_KEY";
  case EV_ABS:
    return "EV_ABS";
  default:
    return "-";
  }
}

static const char *abs_name(unsigned short code) {
  switch (code) {
  case ABS_X:
    return "ABS_X";
  case ABS_Y:
    return "ABS_Y";
  case ABS_Z:
    return "ABS_Z";
  case ABS_RX:
    return "ABS_RX";
  case ABS_RY:
    return "ABS_RY";
  case ABS_RZ:
    return "ABS_RZ";
  case ABS_HAT0X:
    return "ABS_HAT0X";
  case ABS_HAT0Y:
    return "ABS_HAT0Y";
  default:
    return "-";
  }
}

static const char *key_name(unsigned short code) {
  switch (code) {
  case BTN_A:
    return "BTN_A";
  case BTN_B:
    return "BTN_B";
  case BTN_X:
    return "BTN_X";
  case BTN_Y:
    return "BTN_Y";
  case BTN_TL:
    return "BTN_TL";
  case BTN_TR:
    return "BTN_TR";
  case BTN_SELECT:
    return "BTN_SELECT";
  case BTN_START:
    return "BTN_START";
  case BTN_MODE:
    return "BTN_MODE";
  case BTN_THUMBL:
    return "BTN_THUMBL";
  case BTN_THUMBR:
    return "BTN_THUMBR";
  default:
    return "-";
  }
}

static const char *js_type_name(unsigned char type) {
  unsigned char base = type & (unsigned char)~JS_EVENT_INIT;

  if (base == JS_EVENT_AXIS) {
    return (type & JS_EVENT_INIT) ? "JS_AXIS_INIT" : "JS_AXIS";
  }
  if (base == JS_EVENT_BUTTON) {
    return (type & JS_EVENT_INIT) ? "JS_BUTTON_INIT" : "JS_BUTTON";
  }
  return (type & JS_EVENT_INIT) ? "JS_UNKNOWN_INIT" : "JS_UNKNOWN";
}

static void print_js_device_info(int fd, const char *path) {
#if defined(PLUMOS_HAS_LINUX_JOYSTICK)
  unsigned char axes = 0;
  unsigned char buttons = 0;
  char name[128];
  int have_name;
  int have_axes;
  int have_buttons;

  memset(name, 0, sizeof(name));
  have_name = ioctl(fd, JSIOCGNAME(sizeof(name)), name) >= 0;
  have_axes = ioctl(fd, JSIOCGAXES, &axes) >= 0;
  have_buttons = ioctl(fd, JSIOCGBUTTONS, &buttons) >= 0;

  printf("js info path=%s name=", path);
  if (have_name) {
    printf("\"%s\"", name);
  } else {
    printf("-");
  }
  printf(" axes=");
  if (have_axes) {
    printf("%u", (unsigned int)axes);
  } else {
    printf("-");
  }
  printf(" buttons=");
  if (have_buttons) {
    printf("%u", (unsigned int)buttons);
  } else {
    printf("-");
  }
  printf("\n");
#else
  (void)fd;
  printf("js info path=%s unavailable=linux_joystick_headers_missing\n", path);
#endif
}

static void print_evdev_device_info(int fd, const char *path) {
#if defined(PLUMOS_HAS_LINUX_INPUT)
  char name[128];

  memset(name, 0, sizeof(name));
  if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
    printf("evdev info path=%s name=\"%s\"\n", path, name);
  } else {
    printf("evdev info path=%s name=- errno=%d %s\n", path, errno, strerror(errno));
  }
#else
  (void)fd;
  printf("evdev info path=%s unavailable=linux_input_headers_missing\n", path);
#endif
}

static void usage(const char *argv0) {
  printf("Usage: %s [--js PATH] [--event PATH] [--name NAME] [--timeout-ms MS] [--no-js] [--no-event]\n",
         argv0);
  printf("Read Linux joystick and evdev events from a plumOS virtual input device.\n");
}

int main(int argc, char **argv) {
  const char *js_path = DEFAULT_JS_PATH;
  const char *event_path_arg = NULL;
  const char *device_name = DEFAULT_DEVICE_NAME;
  char event_path[PATH_MAX];
  int timeout_ms = DEFAULT_TIMEOUT_MS;
  int no_js = 0;
  int no_event = 0;
  int js_fd = -1;
  int event_fd = -1;
  int js_events = 0;
  int evdev_events = 0;
  long long deadline;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--js") == 0 && i + 1 < argc) {
      js_path = argv[++i];
    } else if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
      event_path_arg = argv[++i];
    } else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
      device_name = argv[++i];
    } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--timeout-ms", argv[++i], 0, 300000, &timeout_ms)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--no-js") == 0) {
      no_js = 1;
    } else if (strcmp(argv[i], "--no-event") == 0) {
      no_event = 1;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  if (event_path_arg) {
    copy_string(event_path, sizeof(event_path), event_path_arg);
  } else if (!discover_event_by_name(device_name, event_path, sizeof(event_path))) {
    copy_string(event_path, sizeof(event_path), DEFAULT_EVENT_PATH);
  }

  printf("plumOS joystick reader\n");
  printf("js=%s event=%s name=\"%s\" timeout_ms=%d no_js=%s no_event=%s\n",
         js_path, event_path, device_name, timeout_ms, no_js ? "yes" : "no",
         no_event ? "yes" : "no");

  if (!no_js) {
    js_fd = open(js_path, O_RDONLY | O_NONBLOCK);
    if (js_fd < 0) {
      printf("js path=%s open=no errno=%d %s\n", js_path, errno, strerror(errno));
    } else {
      printf("js path=%s open=yes\n", js_path);
      print_js_device_info(js_fd, js_path);
    }
  }

  if (!no_event) {
    event_fd = open(event_path, O_RDONLY | O_NONBLOCK);
    if (event_fd < 0) {
      printf("evdev path=%s open=no errno=%d %s\n", event_path, errno, strerror(errno));
    } else {
      printf("evdev path=%s open=yes\n", event_path);
      print_evdev_device_info(event_fd, event_path);
    }
  }

  if (js_fd < 0 && event_fd < 0) {
    return 1;
  }

  deadline = now_ms() + timeout_ms;
  while (now_ms() < deadline) {
    struct pollfd pfds[2];
    int nfds = 0;
    int pr;

    if (js_fd >= 0) {
      pfds[nfds].fd = js_fd;
      pfds[nfds].events = POLLIN;
      pfds[nfds].revents = 0;
      nfds++;
    }
    if (event_fd >= 0) {
      pfds[nfds].fd = event_fd;
      pfds[nfds].events = POLLIN;
      pfds[nfds].revents = 0;
      nfds++;
    }

    pr = poll(pfds, nfds, 100);
    if (pr < 0) {
      if (errno == EINTR) {
        continue;
      }
      printf("poll=no errno=%d %s\n", errno, strerror(errno));
      break;
    }
    if (pr == 0) {
      continue;
    }

    for (int i = 0; i < nfds; i++) {
      if (!(pfds[i].revents & POLLIN)) {
        continue;
      }
      if (pfds[i].fd == js_fd) {
        for (;;) {
          struct js_event ev;
          ssize_t n = read(js_fd, &ev, sizeof(ev));
          if (n == (ssize_t)sizeof(ev)) {
            js_events++;
            printf("js event index=%d time=%u type=0x%02x type_name=%s number=%u value=%d\n",
                   js_events, ev.time, ev.type, js_type_name(ev.type), ev.number,
                   ev.value);
          } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
          } else if (n < 0 && errno == EINTR) {
            continue;
          } else {
            break;
          }
        }
      } else if (pfds[i].fd == event_fd) {
        for (;;) {
          struct input_event ev;
          ssize_t n = read(event_fd, &ev, sizeof(ev));
          if (n == (ssize_t)sizeof(ev)) {
            evdev_events++;
            if (ev.type == EV_ABS) {
              printf("evdev event index=%d type=%u type_name=%s code=%u axis=%s value=%d\n",
                     evdev_events, ev.type, event_type_name(ev.type), ev.code,
                     abs_name(ev.code), ev.value);
            } else if (ev.type == EV_KEY) {
              printf("evdev event index=%d type=%u type_name=%s code=%u key=%s value=%d\n",
                     evdev_events, ev.type, event_type_name(ev.type), ev.code,
                     key_name(ev.code), ev.value);
            } else {
              printf("evdev event index=%d type=%u type_name=%s code=%u value=%d\n",
                     evdev_events, ev.type, event_type_name(ev.type), ev.code, ev.value);
            }
          } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
          } else if (n < 0 && errno == EINTR) {
            continue;
          } else {
            break;
          }
        }
      }
    }
    fflush(stdout);
  }

  if (js_fd >= 0) {
    close(js_fd);
  }
  if (event_fd >= 0) {
    close(event_fd);
  }
  printf("summary js_events=%d evdev_events=%d timeout_ms=%d\n", js_events, evdev_events,
         timeout_ms);
  return 0;
}
