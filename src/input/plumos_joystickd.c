#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<linux/input.h>) && __has_include(<linux/uinput.h>)
#include <linux/input.h>
#include <linux/uinput.h>
#define PLUMOS_HAS_UINPUT 1
#endif
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef CRTSCTS
#define CRTSCTS 0
#endif

#ifndef PLUMOS_HAS_UINPUT
#define EV_SYN 0x00
#define EV_ABS 0x03
#define SYN_REPORT 0
#define ABS_X 0
#define ABS_Y 1
struct input_event {
  struct timeval time;
  unsigned short type;
  unsigned short code;
  int value;
};
#endif

#ifndef BUS_USB
#define BUS_USB 0x03
#endif

#define DEFAULT_SERIAL_PATH "/dev/ttyS0"
#define DEFAULT_CALIBRATION_PATH "/config/joypad.config"
#define DEFAULT_UINPUT_PATH "/dev/uinput"
#define DEFAULT_BAUD 9600
#define DEFAULT_DEADZONE_RAW 8
#define NORMALIZED_MIN (-32768)
#define NORMALIZED_MAX 32767
#define MAX_CHUNK 128

enum axis_source {
  AXIS_YL = 0,
  AXIS_XL = 1,
  AXIS_YR = 2,
  AXIS_XR = 3
};

struct calibration {
  int x_min;
  int x_max;
  int x_zero;
  int y_min;
  int y_max;
  int y_zero;
};

struct config {
  const char *serial_path;
  const char *calibration_path;
  const char *uinput_path;
  int baud;
  int timeout_ms;
  int no_uinput;
  int verbose;
  int print_every;
  int deadzone_raw;
  enum axis_source x_source;
  enum axis_source y_source;
  int invert_x;
  int invert_y;
};

struct joy_frame {
  int axis[4];
};

static volatile sig_atomic_t g_stop = 0;

static void handle_signal(int signo) {
  (void)signo;
  g_stop = 1;
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

static speed_t baud_to_constant(int baud) {
  switch (baud) {
  case 1200:
    return B1200;
  case 2400:
    return B2400;
  case 4800:
    return B4800;
  case 9600:
    return B9600;
  case 19200:
    return B19200;
  case 38400:
    return B38400;
#ifdef B57600
  case 57600:
    return B57600;
#endif
#ifdef B115200
  case 115200:
    return B115200;
#endif
  default:
    return (speed_t)0;
  }
}

static const char *axis_source_name(enum axis_source source) {
  switch (source) {
  case AXIS_YL:
    return "axisYL";
  case AXIS_XL:
    return "axisXL";
  case AXIS_YR:
    return "axisYR";
  case AXIS_XR:
    return "axisXR";
  default:
    return "-";
  }
}

static int parse_axis_source(const char *value, enum axis_source *out) {
  if (strcmp(value, "axisYL") == 0 || strcmp(value, "yl") == 0) {
    *out = AXIS_YL;
    return 1;
  }
  if (strcmp(value, "axisXL") == 0 || strcmp(value, "xl") == 0) {
    *out = AXIS_XL;
    return 1;
  }
  if (strcmp(value, "axisYR") == 0 || strcmp(value, "yr") == 0) {
    *out = AXIS_YR;
    return 1;
  }
  if (strcmp(value, "axisXR") == 0 || strcmp(value, "xr") == 0) {
    *out = AXIS_XR;
    return 1;
  }
  fprintf(stderr, "error: invalid axis source: %s\n", value);
  return 0;
}

static void make_raw_8n1(struct termios *tio, speed_t speed) {
  tio->c_iflag &= (tcflag_t) ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL |
                               IXON | IXOFF | IXANY);
  tio->c_oflag &= (tcflag_t) ~OPOST;
  tio->c_lflag &= (tcflag_t) ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  tio->c_cflag &= (tcflag_t) ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
  tio->c_cflag |= (tcflag_t)(CS8 | CLOCAL | CREAD);
  tio->c_cc[VMIN] = 0;
  tio->c_cc[VTIME] = 0;
  cfsetispeed(tio, speed);
  cfsetospeed(tio, speed);
}

static void calibration_defaults(struct calibration *cal) {
  cal->x_min = 14;
  cal->x_max = 235;
  cal->x_zero = 126;
  cal->y_min = 18;
  cal->y_max = 232;
  cal->y_zero = 130;
}

static int load_calibration(const char *path, struct calibration *cal) {
  FILE *f;
  char line[128];
  int seen = 0;

  calibration_defaults(cal);
  f = fopen(path, "rb");
  if (!f) {
    printf("calibration path=%s open=no errno=%d %s using_defaults=yes\n", path, errno,
           strerror(errno));
    return 0;
  }
  while (fgets(line, sizeof(line), f)) {
    int value;
    if (sscanf(line, "x_min=%d", &value) == 1) {
      cal->x_min = value;
      seen++;
    } else if (sscanf(line, "x_max=%d", &value) == 1) {
      cal->x_max = value;
      seen++;
    } else if (sscanf(line, "x_zero=%d", &value) == 1) {
      cal->x_zero = value;
      seen++;
    } else if (sscanf(line, "y_min=%d", &value) == 1) {
      cal->y_min = value;
      seen++;
    } else if (sscanf(line, "y_max=%d", &value) == 1) {
      cal->y_max = value;
      seen++;
    } else if (sscanf(line, "y_zero=%d", &value) == 1) {
      cal->y_zero = value;
      seen++;
    }
  }
  fclose(f);
  printf("calibration path=%s open=yes fields=%d x_min=%d x_max=%d x_zero=%d y_min=%d y_max=%d y_zero=%d\n",
         path, seen, cal->x_min, cal->x_max, cal->x_zero, cal->y_min, cal->y_max,
         cal->y_zero);
  return seen >= 6;
}

static int clamp_int(int value, int min_value, int max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static int normalize_axis(int raw, int min_value, int max_value, int zero, int deadzone_raw,
                          int invert) {
  int delta = raw - zero;
  int denom;
  long scaled;

  if (delta >= -deadzone_raw && delta <= deadzone_raw) {
    return 0;
  }

  if (delta < 0) {
    denom = zero - min_value;
    if (denom <= 0) {
      denom = 1;
    }
    scaled = ((long)delta * (long)(-NORMALIZED_MIN)) / (long)denom;
  } else {
    denom = max_value - zero;
    if (denom <= 0) {
      denom = 1;
    }
    scaled = ((long)delta * (long)NORMALIZED_MAX) / (long)denom;
  }

  scaled = clamp_int((int)scaled, NORMALIZED_MIN, NORMALIZED_MAX);
  if (invert) {
    scaled = -scaled;
  }
  return (int)scaled;
}

static int open_serial(const char *path, int baud, struct termios *old_tio, int *have_old_tio) {
  int fd;
  speed_t speed;
  struct termios new_tio;

  speed = baud_to_constant(baud);
  if (speed == (speed_t)0) {
    fprintf(stderr, "error: unsupported baud: %d\n", baud);
    return -1;
  }

  fd = open(path, O_RDONLY | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    printf("serial path=%s open=no errno=%d %s\n", path, errno, strerror(errno));
    return -1;
  }
  if (tcgetattr(fd, old_tio) == 0) {
    *have_old_tio = 1;
    new_tio = *old_tio;
    make_raw_8n1(&new_tio, speed);
    if (tcsetattr(fd, TCSANOW, &new_tio) != 0) {
      printf("serial path=%s tcsetattr=no errno=%d %s\n", path, errno, strerror(errno));
    } else {
      printf("serial path=%s tcsetattr=yes raw_8n1=yes\n", path);
    }
  } else {
    *have_old_tio = 0;
    printf("serial path=%s tcgetattr=no errno=%d %s\n", path, errno, strerror(errno));
  }
  return fd;
}

static int read_frames(int serial_fd, unsigned char *window, int *window_count,
                       struct joy_frame *out, int *frames_out) {
  int got_frame = 0;

  for (;;) {
    unsigned char buf[MAX_CHUNK];
    ssize_t n = read(serial_fd, buf, sizeof(buf));
    if (n > 0) {
      for (ssize_t i = 0; i < n; i++) {
        if (*window_count < 6) {
          window[*window_count] = buf[i];
          (*window_count)++;
        } else {
          memmove(window, window + 1, 5);
          window[5] = buf[i];
        }

        if (*window_count == 6 && window[0] == 0xff && window[5] == 0xfe) {
          out->axis[AXIS_YL] = window[1];
          out->axis[AXIS_XL] = window[2];
          out->axis[AXIS_YR] = window[3];
          out->axis[AXIS_XR] = window[4];
          (*frames_out)++;
          got_frame = 1;
        }
      }
    } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      break;
    } else if (n < 0 && errno == EINTR) {
      continue;
    } else {
      break;
    }
  }
  return got_frame;
}

#if defined(PLUMOS_HAS_UINPUT)
static int write_input_event(int fd, unsigned short type, unsigned short code, int value) {
  struct input_event ev;

  memset(&ev, 0, sizeof(ev));
  gettimeofday(&ev.time, NULL);
  ev.type = type;
  ev.code = code;
  ev.value = value;
  if (write(fd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev)) {
    return 0;
  }
  return 1;
}

static int create_uinput_device(const char *path) {
  int fd;
  struct uinput_user_dev dev;

  fd = open(path, O_WRONLY | O_NONBLOCK);
  if (fd < 0) {
    printf("uinput path=%s open=no errno=%d %s\n", path, errno, strerror(errno));
    return -1;
  }
  if (ioctl(fd, UI_SET_EVBIT, EV_ABS) != 0 ||
      ioctl(fd, UI_SET_ABSBIT, ABS_X) != 0 ||
      ioctl(fd, UI_SET_ABSBIT, ABS_Y) != 0) {
    printf("uinput path=%s setup=no errno=%d %s\n", path, errno, strerror(errno));
    close(fd);
    return -1;
  }

  memset(&dev, 0, sizeof(dev));
  snprintf(dev.name, sizeof(dev.name), "plumOS A30 Analog Stick");
  dev.id.bustype = BUS_USB;
  dev.id.vendor = 0x706c;
  dev.id.product = 0x4130;
  dev.id.version = 1;
  dev.absmin[ABS_X] = NORMALIZED_MIN;
  dev.absmax[ABS_X] = NORMALIZED_MAX;
  dev.absflat[ABS_X] = 2048;
  dev.absfuzz[ABS_X] = 256;
  dev.absmin[ABS_Y] = NORMALIZED_MIN;
  dev.absmax[ABS_Y] = NORMALIZED_MAX;
  dev.absflat[ABS_Y] = 2048;
  dev.absfuzz[ABS_Y] = 256;

  if (write(fd, &dev, sizeof(dev)) != (ssize_t)sizeof(dev)) {
    printf("uinput path=%s write_device=no errno=%d %s\n", path, errno, strerror(errno));
    close(fd);
    return -1;
  }
  if (ioctl(fd, UI_DEV_CREATE) != 0) {
    printf("uinput path=%s create=no errno=%d %s\n", path, errno, strerror(errno));
    close(fd);
    return -1;
  }
  usleep(100000);
  printf("uinput path=%s create=yes name=\"plumOS A30 Analog Stick\" buttons=none axes=ABS_X,ABS_Y\n",
         path);
  return fd;
}

static void destroy_uinput_device(int fd) {
  if (fd >= 0) {
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
  }
}

static int emit_axes(int fd, int x, int y) {
  return write_input_event(fd, EV_ABS, ABS_X, x) &&
         write_input_event(fd, EV_ABS, ABS_Y, y) &&
         write_input_event(fd, EV_SYN, SYN_REPORT, 0);
}
#else
static int create_uinput_device(const char *path) {
  printf("uinput path=%s create=no reason=linux_uinput_headers_unavailable\n", path);
  return -1;
}

static void destroy_uinput_device(int fd) {
  (void)fd;
}

static int emit_axes(int fd, int x, int y) {
  (void)fd;
  (void)x;
  (void)y;
  return 0;
}
#endif

static void usage(const char *argv0) {
  printf("Usage: %s [options]\n", argv0);
  printf("Options:\n");
  printf("  --serial PATH          Serial raw stick path. Default: %s\n", DEFAULT_SERIAL_PATH);
  printf("  --calibration PATH     Calibration path. Default: %s\n", DEFAULT_CALIBRATION_PATH);
  printf("  --uinput PATH          uinput path. Default: %s\n", DEFAULT_UINPUT_PATH);
  printf("  --timeout-ms MS        Stop after MS. Default: 0, run until signal.\n");
  printf("  --no-uinput            Do not create a virtual input device.\n");
  printf("  --x-source NAME        axisYL, axisXL, axisYR, or axisXR. Default: axisXR.\n");
  printf("  --y-source NAME        axisYL, axisXL, axisYR, or axisXR. Default: axisYR.\n");
  printf("  --invert-x             Invert normalized ABS_X.\n");
  printf("  --invert-y             Invert normalized ABS_Y.\n");
  printf("  --deadzone-raw N       Raw deadzone around zero. Default: %d.\n", DEFAULT_DEADZONE_RAW);
  printf("  --print-every N        Print every Nth frame. Default: 0, summary only.\n");
  printf("  --verbose              Print startup details.\n");
}

int main(int argc, char **argv) {
  struct config cfg;
  struct calibration cal;
  struct termios old_tio;
  int have_old_tio = 0;
  int serial_fd;
  int uinput_fd = -1;
  long long start_ms;
  long long deadline = 0;
  unsigned char window[6];
  int window_count = 0;
  int frames = 0;
  int emitted = 0;
  int last_x = 999999;
  int last_y = 999999;

  cfg.serial_path = DEFAULT_SERIAL_PATH;
  cfg.calibration_path = DEFAULT_CALIBRATION_PATH;
  cfg.uinput_path = DEFAULT_UINPUT_PATH;
  cfg.baud = DEFAULT_BAUD;
  cfg.timeout_ms = 0;
  cfg.no_uinput = 0;
  cfg.verbose = 0;
  cfg.print_every = 0;
  cfg.deadzone_raw = DEFAULT_DEADZONE_RAW;
  cfg.x_source = AXIS_XR;
  cfg.y_source = AXIS_YR;
  cfg.invert_x = 0;
  cfg.invert_y = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--serial") == 0 && i + 1 < argc) {
      cfg.serial_path = argv[++i];
    } else if (strcmp(argv[i], "--calibration") == 0 && i + 1 < argc) {
      cfg.calibration_path = argv[++i];
    } else if (strcmp(argv[i], "--uinput") == 0 && i + 1 < argc) {
      cfg.uinput_path = argv[++i];
    } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--timeout-ms", argv[++i], 0, 86400000, &cfg.timeout_ms)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--baud") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--baud", argv[++i], 1200, 115200, &cfg.baud)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--deadzone-raw") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--deadzone-raw", argv[++i], 0, 127, &cfg.deadzone_raw)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--print-every") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--print-every", argv[++i], 0, 10000, &cfg.print_every)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--x-source") == 0 && i + 1 < argc) {
      if (!parse_axis_source(argv[++i], &cfg.x_source)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--y-source") == 0 && i + 1 < argc) {
      if (!parse_axis_source(argv[++i], &cfg.y_source)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--invert-x") == 0) {
      cfg.invert_x = 1;
    } else if (strcmp(argv[i], "--invert-y") == 0) {
      cfg.invert_y = 1;
    } else if (strcmp(argv[i], "--no-uinput") == 0) {
      cfg.no_uinput = 1;
    } else if (strcmp(argv[i], "--verbose") == 0) {
      cfg.verbose = 1;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  printf("plumOS joystickd\n");
  printf("serial=%s baud=%d calibration=%s uinput=%s no_uinput=%s x_source=%s y_source=%s invert_x=%s invert_y=%s deadzone_raw=%d timeout_ms=%d\n",
         cfg.serial_path, cfg.baud, cfg.calibration_path, cfg.uinput_path,
         cfg.no_uinput ? "yes" : "no", axis_source_name(cfg.x_source),
         axis_source_name(cfg.y_source), cfg.invert_x ? "yes" : "no",
         cfg.invert_y ? "yes" : "no", cfg.deadzone_raw, cfg.timeout_ms);

  load_calibration(cfg.calibration_path, &cal);
  serial_fd = open_serial(cfg.serial_path, cfg.baud, &old_tio, &have_old_tio);
  if (serial_fd < 0) {
    return 1;
  }

  if (!cfg.no_uinput) {
    uinput_fd = create_uinput_device(cfg.uinput_path);
    if (uinput_fd < 0) {
      if (have_old_tio) {
        tcsetattr(serial_fd, TCSANOW, &old_tio);
      }
      close(serial_fd);
      return 1;
    }
  }

  memset(window, 0, sizeof(window));
  start_ms = now_ms();
  if (cfg.timeout_ms > 0) {
    deadline = start_ms + cfg.timeout_ms;
  }

  while (!g_stop) {
    struct pollfd pfd;
    int pr;
    struct joy_frame frame;

    if (cfg.timeout_ms > 0 && now_ms() >= deadline) {
      break;
    }

    pfd.fd = serial_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pr = poll(&pfd, 1, 100);
    if (pr < 0) {
      if (errno == EINTR) {
        continue;
      }
      printf("serial path=%s poll=no errno=%d %s\n", cfg.serial_path, errno,
             strerror(errno));
      break;
    }
    if (pr == 0 || !(pfd.revents & POLLIN)) {
      continue;
    }

    memset(&frame, 0, sizeof(frame));
    if (read_frames(serial_fd, window, &window_count, &frame, &frames)) {
      int raw_x = frame.axis[cfg.x_source];
      int raw_y = frame.axis[cfg.y_source];
      int x = normalize_axis(raw_x, cal.x_min, cal.x_max, cal.x_zero, cfg.deadzone_raw,
                             cfg.invert_x);
      int y = normalize_axis(raw_y, cal.y_min, cal.y_max, cal.y_zero, cfg.deadzone_raw,
                             cfg.invert_y);
      if (!cfg.no_uinput && (x != last_x || y != last_y)) {
        if (emit_axes(uinput_fd, x, y)) {
          emitted++;
        }
      }
      last_x = x;
      last_y = y;
      if (cfg.print_every > 0 && frames % cfg.print_every == 0) {
        printf("frame=%d raw axisYL=%d axisXL=%d axisYR=%d axisXR=%d normalized x=%d y=%d emitted=%d\n",
               frames, frame.axis[AXIS_YL], frame.axis[AXIS_XL], frame.axis[AXIS_YR],
               frame.axis[AXIS_XR], x, y, emitted);
      }
    }
  }

  if (have_old_tio) {
    tcsetattr(serial_fd, TCSANOW, &old_tio);
  }
  close(serial_fd);
  destroy_uinput_device(uinput_fd);
  printf("summary frames=%d emitted=%d last_x=%d last_y=%d duration_ms=%lld\n", frames,
         emitted, last_x == 999999 ? 0 : last_x, last_y == 999999 ? 0 : last_y,
         now_ms() - start_ms);
  return 0;
}
