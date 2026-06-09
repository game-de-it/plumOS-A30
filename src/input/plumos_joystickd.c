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
#define EV_KEY 0x01
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

#ifndef KEY_ESC
#define KEY_ESC 1
#endif
#ifndef KEY_BACKSPACE
#define KEY_BACKSPACE 14
#endif
#ifndef KEY_TAB
#define KEY_TAB 15
#endif
#ifndef KEY_E
#define KEY_E 18
#endif
#ifndef KEY_T
#define KEY_T 20
#endif
#ifndef KEY_LEFTCTRL
#define KEY_LEFTCTRL 29
#endif
#ifndef KEY_LEFTSHIFT
#define KEY_LEFTSHIFT 42
#endif
#ifndef KEY_LEFTALT
#define KEY_LEFTALT 56
#endif
#ifndef KEY_SPACE
#define KEY_SPACE 57
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
#ifndef KEY_N
#define KEY_N 49
#endif
#ifndef KEY_F1
#define KEY_F1 59
#endif
#ifndef KEY_F7
#define KEY_F7 65
#endif
#ifndef KEY_F9
#define KEY_F9 67
#endif
#ifndef ABS_Z
#define ABS_Z 2
#endif
#ifndef ABS_RX
#define ABS_RX 3
#endif
#ifndef ABS_RY
#define ABS_RY 4
#endif
#ifndef ABS_RZ
#define ABS_RZ 5
#endif
#ifndef ABS_HAT0X
#define ABS_HAT0X 16
#endif
#ifndef ABS_HAT0Y
#define ABS_HAT0Y 17
#endif
#ifndef BTN_A
#define BTN_A 304
#endif
#ifndef BTN_B
#define BTN_B 305
#endif
#ifndef BTN_X
#define BTN_X 307
#endif
#ifndef BTN_Y
#define BTN_Y 308
#endif
#ifndef BTN_TL
#define BTN_TL 310
#endif
#ifndef BTN_TR
#define BTN_TR 311
#endif
#ifndef BTN_TL2
#define BTN_TL2 312
#endif
#ifndef BTN_TR2
#define BTN_TR2 313
#endif
#ifndef BTN_SELECT
#define BTN_SELECT 314
#endif
#ifndef BTN_START
#define BTN_START 315
#endif
#ifndef BTN_MODE
#define BTN_MODE 316
#endif
#ifndef BTN_THUMBL
#define BTN_THUMBL 317
#endif
#ifndef BTN_THUMBR
#define BTN_THUMBR 318
#endif
#ifndef BUS_USB
#define BUS_USB 0x03
#endif

#define DEFAULT_SERIAL_PATH "/dev/ttyS0"
#define DEFAULT_CALIBRATION_PATH "/config/joypad.config"
#define DEFAULT_UINPUT_PATH "/dev/uinput"
#define DEFAULT_BUTTON_EVENT_PATH "/dev/input/event3"
#define DEFAULT_BAUD 9600
#define DEFAULT_DEADZONE_RAW 8
#define NORMALIZED_MIN (-32768)
#define NORMALIZED_MAX 32767
#define MAX_CHUNK 128
#define TRIGGER_RELEASED (-32767)
#define TRIGGER_PRESSED 32767

enum axis_source {
  AXIS_YL = 0,
  AXIS_XL = 1,
  AXIS_YR = 2,
  AXIS_XR = 3
};

enum device_mode {
  DEVICE_MODE_ANALOG = 0,
  DEVICE_MODE_XBOX = 1,
  DEVICE_MODE_KEYBOARD = 2
};

enum trigger_mode {
  TRIGGER_MODE_BUTTONS = 0,
  TRIGGER_MODE_AXES = 1
};

enum shoulder_layout {
  SHOULDER_LAYOUT_STANDARD = 0,
  SHOULDER_LAYOUT_USER = 1
};

enum shoulder_role {
  SHOULDER_ROLE_NONE = 0,
  SHOULDER_ROLE_L1,
  SHOULDER_ROLE_R1,
  SHOULDER_ROLE_L2,
  SHOULDER_ROLE_R2
};

enum keyboard_profile {
  KEYBOARD_PROFILE_PASSTHROUGH = 0,
  KEYBOARD_PROFILE_DOSBOX = 1,
  KEYBOARD_PROFILE_DIGGER = 2
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
  const char *button_event_path;
  enum device_mode device_mode;
  enum trigger_mode trigger_mode;
  enum shoulder_layout shoulder_layout;
  enum keyboard_profile keyboard_profile;
  int baud;
  int timeout_ms;
  int no_uinput;
  int no_buttons;
  int verbose;
  int print_every;
  int deadzone_raw;
  enum axis_source x_source;
  enum axis_source y_source;
  int invert_x;
  int invert_y;
  int function_button_code;
};

struct joy_frame {
  int axis[4];
};

struct button_state {
  int up;
  int down;
  int left;
  int right;
  int hat_x;
  int hat_y;
  int lt;
  int rt;
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

static const char *device_mode_name(enum device_mode mode) {
  switch (mode) {
  case DEVICE_MODE_ANALOG:
    return "analog";
  case DEVICE_MODE_XBOX:
    return "xbox";
  case DEVICE_MODE_KEYBOARD:
    return "keyboard";
  default:
    return "-";
  }
}

static const char *keyboard_profile_name(enum keyboard_profile profile) {
  switch (profile) {
  case KEYBOARD_PROFILE_PASSTHROUGH:
    return "passthrough";
  case KEYBOARD_PROFILE_DOSBOX:
    return "dosbox";
  case KEYBOARD_PROFILE_DIGGER:
    return "digger";
  default:
    return "-";
  }
}

static const char *trigger_mode_name(enum trigger_mode mode) {
  switch (mode) {
  case TRIGGER_MODE_BUTTONS:
    return "buttons";
  case TRIGGER_MODE_AXES:
    return "axes";
  default:
    return "-";
  }
}

static const char *shoulder_layout_name(enum shoulder_layout layout) {
  switch (layout) {
  case SHOULDER_LAYOUT_STANDARD:
    return "standard";
  case SHOULDER_LAYOUT_USER:
    return "user";
  default:
    return "-";
  }
}

static int parse_device_mode(const char *value, enum device_mode *out) {
  if (strcmp(value, "analog") == 0 || strcmp(value, "axes") == 0) {
    *out = DEVICE_MODE_ANALOG;
    return 1;
  }
  if (strcmp(value, "xbox") == 0 || strcmp(value, "gamepad") == 0 ||
      strcmp(value, "composite") == 0) {
    *out = DEVICE_MODE_XBOX;
    return 1;
  }
  if (strcmp(value, "keyboard") == 0 || strcmp(value, "kbd") == 0 ||
      strcmp(value, "keys") == 0) {
    *out = DEVICE_MODE_KEYBOARD;
    return 1;
  }
  fprintf(stderr, "error: invalid device mode: %s\n", value);
  return 0;
}

static int parse_keyboard_profile(const char *value, enum keyboard_profile *out) {
  if (strcmp(value, "passthrough") == 0 || strcmp(value, "raw") == 0) {
    *out = KEYBOARD_PROFILE_PASSTHROUGH;
    return 1;
  }
  if (strcmp(value, "dosbox") == 0 || strcmp(value, "dos") == 0) {
    *out = KEYBOARD_PROFILE_DOSBOX;
    return 1;
  }
  if (strcmp(value, "digger") == 0) {
    *out = KEYBOARD_PROFILE_DIGGER;
    return 1;
  }
  fprintf(stderr, "error: invalid keyboard profile: %s\n", value);
  return 0;
}

static int parse_trigger_mode(const char *value, enum trigger_mode *out) {
  if (strcmp(value, "buttons") == 0 || strcmp(value, "button") == 0 ||
      strcmp(value, "btn") == 0) {
    *out = TRIGGER_MODE_BUTTONS;
    return 1;
  }
  if (strcmp(value, "axes") == 0 || strcmp(value, "axis") == 0 ||
      strcmp(value, "analog") == 0) {
    *out = TRIGGER_MODE_AXES;
    return 1;
  }
  fprintf(stderr, "error: invalid trigger mode: %s\n", value);
  return 0;
}

static int parse_shoulder_layout(const char *value, enum shoulder_layout *out) {
  if (strcmp(value, "standard") == 0 || strcmp(value, "stock") == 0 ||
      strcmp(value, "sdl-a30") == 0) {
    *out = SHOULDER_LAYOUT_STANDARD;
    return 1;
  }
  if (strcmp(value, "user") == 0 || strcmp(value, "swapped") == 0 ||
      strcmp(value, "swap") == 0) {
    *out = SHOULDER_LAYOUT_USER;
    return 1;
  }
  fprintf(stderr, "error: invalid shoulder layout: %s\n", value);
  return 0;
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

static const char *function_button_name(int code) {
  switch (code) {
  case BTN_MODE:
    return "mode";
  case BTN_THUMBL:
    return "thumbl";
  case BTN_THUMBR:
    return "thumbr";
  case -1:
    return "none";
  default:
    return "-";
  }
}

static int parse_function_button(const char *value, int *out) {
  if (strcmp(value, "mode") == 0 || strcmp(value, "guide") == 0) {
    *out = BTN_MODE;
    return 1;
  }
  if (strcmp(value, "thumbl") == 0 || strcmp(value, "leftstick") == 0) {
    *out = BTN_THUMBL;
    return 1;
  }
  if (strcmp(value, "thumbr") == 0 || strcmp(value, "rightstick") == 0) {
    *out = BTN_THUMBR;
    return 1;
  }
  if (strcmp(value, "none") == 0 || strcmp(value, "off") == 0) {
    *out = -1;
    return 1;
  }
  fprintf(stderr, "error: invalid function button: %s\n", value);
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

static int set_abs_bit(int fd, int code) {
  if (ioctl(fd, UI_SET_ABSBIT, code) != 0) {
    return 0;
  }
  return 1;
}

static int set_key_bit(int fd, int code) {
  if (ioctl(fd, UI_SET_KEYBIT, code) != 0) {
    return 0;
  }
  return 1;
}

static void configure_abs_axis(struct uinput_user_dev *dev, int code, int min_value,
                               int max_value, int flat, int fuzz) {
  dev->absmin[code] = min_value;
  dev->absmax[code] = max_value;
  dev->absflat[code] = flat;
  dev->absfuzz[code] = fuzz;
}

static int create_analog_uinput_device(const char *path) {
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
  configure_abs_axis(&dev, ABS_X, NORMALIZED_MIN, NORMALIZED_MAX, 2048, 256);
  configure_abs_axis(&dev, ABS_Y, NORMALIZED_MIN, NORMALIZED_MAX, 2048, 256);

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

static int create_xbox_uinput_device(const struct config *cfg) {
  static const int key_bits[] = {
      BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR,
      BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR,
  };
  static const int trigger_button_bits[] = {
      BTN_TL2, BTN_TR2,
  };
  static const int base_abs_bits[] = {
      ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_HAT0X, ABS_HAT0Y,
  };
  static const int trigger_abs_bits[] = {
      ABS_Z, ABS_RZ,
  };
  const char *path = cfg->uinput_path;
  int fd;
  struct uinput_user_dev dev;

  fd = open(path, O_WRONLY | O_NONBLOCK);
  if (fd < 0) {
    printf("uinput path=%s open=no errno=%d %s\n", path, errno, strerror(errno));
    return -1;
  }
  if (ioctl(fd, UI_SET_EVBIT, EV_KEY) != 0 || ioctl(fd, UI_SET_EVBIT, EV_ABS) != 0) {
    printf("uinput path=%s setup=no errno=%d %s\n", path, errno, strerror(errno));
    close(fd);
    return -1;
  }
  for (size_t i = 0; i < sizeof(key_bits) / sizeof(key_bits[0]); i++) {
    if (!set_key_bit(fd, key_bits[i])) {
      printf("uinput path=%s keybit=%d setup=no errno=%d %s\n", path, key_bits[i], errno,
             strerror(errno));
      close(fd);
      return -1;
    }
  }
  if (cfg->trigger_mode == TRIGGER_MODE_BUTTONS) {
    for (size_t i = 0; i < sizeof(trigger_button_bits) / sizeof(trigger_button_bits[0]); i++) {
      if (!set_key_bit(fd, trigger_button_bits[i])) {
        printf("uinput path=%s keybit=%d setup=no errno=%d %s\n", path,
               trigger_button_bits[i], errno, strerror(errno));
        close(fd);
        return -1;
      }
    }
  }
  for (size_t i = 0; i < sizeof(base_abs_bits) / sizeof(base_abs_bits[0]); i++) {
    if (!set_abs_bit(fd, base_abs_bits[i])) {
      printf("uinput path=%s absbit=%d setup=no errno=%d %s\n", path, base_abs_bits[i], errno,
             strerror(errno));
      close(fd);
      return -1;
    }
  }
  if (cfg->trigger_mode == TRIGGER_MODE_AXES) {
    for (size_t i = 0; i < sizeof(trigger_abs_bits) / sizeof(trigger_abs_bits[0]); i++) {
      if (!set_abs_bit(fd, trigger_abs_bits[i])) {
        printf("uinput path=%s absbit=%d setup=no errno=%d %s\n", path,
               trigger_abs_bits[i], errno, strerror(errno));
        close(fd);
        return -1;
      }
    }
  }

  memset(&dev, 0, sizeof(dev));
  snprintf(dev.name, sizeof(dev.name), "plumOS A30 Gamepad");
  dev.id.bustype = BUS_USB;
  dev.id.vendor = 0x045e;
  dev.id.product = 0x028e;
  dev.id.version = 0x045e;
  configure_abs_axis(&dev, ABS_X, NORMALIZED_MIN, NORMALIZED_MAX, 2048, 256);
  configure_abs_axis(&dev, ABS_Y, NORMALIZED_MIN, NORMALIZED_MAX, 2048, 256);
  configure_abs_axis(&dev, ABS_RX, NORMALIZED_MIN, NORMALIZED_MAX, 2048, 256);
  configure_abs_axis(&dev, ABS_RY, NORMALIZED_MIN, NORMALIZED_MAX, 2048, 256);
  configure_abs_axis(&dev, ABS_HAT0X, -1, 1, 0, 0);
  configure_abs_axis(&dev, ABS_HAT0Y, -1, 1, 0, 0);
  if (cfg->trigger_mode == TRIGGER_MODE_AXES) {
    configure_abs_axis(&dev, ABS_Z, NORMALIZED_MIN, NORMALIZED_MAX, 0, 0);
    configure_abs_axis(&dev, ABS_RZ, NORMALIZED_MIN, NORMALIZED_MAX, 0, 0);
  }

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
  if (cfg->trigger_mode == TRIGGER_MODE_AXES) {
    printf("uinput path=%s create=yes name=\"plumOS A30 Gamepad\" id=045e:028e trigger_mode=axes axes=ABS_X,ABS_Y,ABS_Z,ABS_RX,ABS_RY,ABS_RZ,ABS_HAT0X,ABS_HAT0Y buttons=11\n",
           path);
  } else {
    printf("uinput path=%s create=yes name=\"plumOS A30 Gamepad\" id=045e:028e trigger_mode=buttons axes=ABS_X,ABS_Y,ABS_RX,ABS_RY,ABS_HAT0X,ABS_HAT0Y buttons=13\n",
           path);
  }
  return fd;
}

static int create_keyboard_uinput_device(const struct config *cfg) {
  static const int key_bits[] = {
      KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SPACE, KEY_LEFTCTRL, KEY_LEFTSHIFT,
      KEY_LEFTALT, KEY_ENTER, KEY_RIGHTCTRL, KEY_ESC, KEY_TAB, KEY_BACKSPACE,
      KEY_E, KEY_T, KEY_N, KEY_F1, KEY_F7, KEY_F9,
  };
  const char *path = cfg->uinput_path;
  int fd;
  struct uinput_user_dev dev;

  fd = open(path, O_WRONLY | O_NONBLOCK);
  if (fd < 0) {
    printf("uinput path=%s open=no errno=%d %s\n", path, errno, strerror(errno));
    return -1;
  }
  if (ioctl(fd, UI_SET_EVBIT, EV_KEY) != 0) {
    printf("uinput path=%s setup=no errno=%d %s\n", path, errno, strerror(errno));
    close(fd);
    return -1;
  }
  for (size_t i = 0; i < sizeof(key_bits) / sizeof(key_bits[0]); i++) {
    if (!set_key_bit(fd, key_bits[i])) {
      printf("uinput path=%s keybit=%d setup=no errno=%d %s\n", path, key_bits[i],
             errno, strerror(errno));
      close(fd);
      return -1;
    }
  }

  memset(&dev, 0, sizeof(dev));
  snprintf(dev.name, sizeof(dev.name), "plumOS A30 Keyboard");
  dev.id.bustype = BUS_USB;
  dev.id.vendor = 0x706c;
  dev.id.product = 0x4b42;
  dev.id.version = 1;

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
  printf("uinput path=%s create=yes name=\"plumOS A30 Keyboard\" profile=%s keys=%zu\n",
         path, keyboard_profile_name(cfg->keyboard_profile),
         sizeof(key_bits) / sizeof(key_bits[0]));
  return fd;
}

static int create_uinput_device(const struct config *cfg) {
  if (cfg->device_mode == DEVICE_MODE_XBOX) {
    return create_xbox_uinput_device(cfg);
  }
  if (cfg->device_mode == DEVICE_MODE_KEYBOARD) {
    return create_keyboard_uinput_device(cfg);
  }
  return create_analog_uinput_device(cfg->uinput_path);
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

static int emit_xbox_idle_state(int fd, enum trigger_mode trigger_mode) {
  int ok = write_input_event(fd, EV_ABS, ABS_X, 0) &&
         write_input_event(fd, EV_ABS, ABS_Y, 0) &&
         write_input_event(fd, EV_ABS, ABS_RX, 0) &&
         write_input_event(fd, EV_ABS, ABS_RY, 0) &&
         write_input_event(fd, EV_ABS, ABS_HAT0X, 0) &&
         write_input_event(fd, EV_ABS, ABS_HAT0Y, 0);
  if (trigger_mode == TRIGGER_MODE_AXES) {
    ok = ok &&
         write_input_event(fd, EV_ABS, ABS_Z, TRIGGER_RELEASED) &&
         write_input_event(fd, EV_ABS, ABS_RZ, TRIGGER_RELEASED);
  } else {
    ok = ok &&
         write_input_event(fd, EV_KEY, BTN_TL2, 0) &&
         write_input_event(fd, EV_KEY, BTN_TR2, 0);
  }
  return ok && write_input_event(fd, EV_SYN, SYN_REPORT, 0);
}
#else
static int write_input_event(int fd, unsigned short type, unsigned short code, int value) {
  (void)fd;
  (void)type;
  (void)code;
  (void)value;
  return 0;
}

static int create_uinput_device(const struct config *cfg) {
  printf("uinput path=%s create=no reason=linux_uinput_headers_unavailable\n",
         cfg->uinput_path);
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

static int emit_xbox_idle_state(int fd, enum trigger_mode trigger_mode) {
  (void)fd;
  (void)trigger_mode;
  return 0;
}
#endif

static int open_button_event(const char *path) {
  int fd = open(path, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    printf("button_event path=%s open=no errno=%d %s\n", path, errno, strerror(errno));
    return -1;
  }
  printf("button_event path=%s open=yes\n", path);
  return fd;
}

static int button_hat_value(int negative, int positive) {
  if (negative && !positive) {
    return -1;
  }
  if (positive && !negative) {
    return 1;
  }
  return 0;
}

static int emit_button_state(int uinput_fd, int code, int pressed) {
  return write_input_event(uinput_fd, EV_KEY, (unsigned short)code, pressed ? 1 : 0) &&
         write_input_event(uinput_fd, EV_SYN, SYN_REPORT, 0);
}

static int emit_abs_state(int uinput_fd, int code, int value) {
  return write_input_event(uinput_fd, EV_ABS, (unsigned short)code, value) &&
         write_input_event(uinput_fd, EV_SYN, SYN_REPORT, 0);
}

static enum shoulder_role map_key_to_shoulder_role(const struct config *cfg, int key_code) {
  if (cfg->shoulder_layout == SHOULDER_LAYOUT_USER) {
    switch (key_code) {
    case KEY_E:
      return SHOULDER_ROLE_L1;
    case KEY_T:
      return SHOULDER_ROLE_R1;
    case KEY_TAB:
      return SHOULDER_ROLE_L2;
    case KEY_BACKSPACE:
      return SHOULDER_ROLE_R2;
    default:
      return SHOULDER_ROLE_NONE;
    }
  }

  switch (key_code) {
  case KEY_TAB:
    return SHOULDER_ROLE_L1;
  case KEY_BACKSPACE:
    return SHOULDER_ROLE_R1;
  case KEY_E:
    return SHOULDER_ROLE_L2;
  case KEY_T:
    return SHOULDER_ROLE_R2;
  default:
    return SHOULDER_ROLE_NONE;
  }
}

static int map_key_to_button(const struct config *cfg, int key_code) {
  enum shoulder_role shoulder = map_key_to_shoulder_role(cfg, key_code);
  switch (shoulder) {
  case SHOULDER_ROLE_L1:
    return BTN_TL;
  case SHOULDER_ROLE_R1:
    return BTN_TR;
  case SHOULDER_ROLE_L2:
    return cfg->trigger_mode == TRIGGER_MODE_BUTTONS ? BTN_TL2 : -1;
  case SHOULDER_ROLE_R2:
    return cfg->trigger_mode == TRIGGER_MODE_BUTTONS ? BTN_TR2 : -1;
  default:
    break;
  }

  switch (key_code) {
  case KEY_SPACE:
    return BTN_A;
  case KEY_LEFTCTRL:
    return BTN_B;
  case KEY_LEFTSHIFT:
    return BTN_X;
  case KEY_LEFTALT:
    return BTN_Y;
  case KEY_ENTER:
    return BTN_START;
  case KEY_RIGHTCTRL:
    return BTN_SELECT;
  case KEY_ESC:
    return cfg->function_button_code;
  default:
    return -1;
  }
}

static int map_key_to_keyboard(const struct config *cfg, int key_code) {
  switch (cfg->keyboard_profile) {
  case KEYBOARD_PROFILE_PASSTHROUGH:
    return key_code;
  case KEYBOARD_PROFILE_DIGGER:
    switch (key_code) {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
      return key_code;
    case KEY_SPACE:
      return KEY_F1;
    case KEY_LEFTCTRL:
      return KEY_ESC;
    case KEY_LEFTSHIFT:
      return KEY_SPACE;
    case KEY_LEFTALT:
      return KEY_F9;
    case KEY_ENTER:
      return KEY_ENTER;
    case KEY_RIGHTCTRL:
      return KEY_F7;
    case KEY_ESC:
      return KEY_ESC;
    case KEY_TAB:
      return KEY_N;
    case KEY_BACKSPACE:
      return KEY_F9;
    default:
      return -1;
    }
  case KEYBOARD_PROFILE_DOSBOX:
  default:
    switch (key_code) {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
      return key_code;
    case KEY_SPACE:
      return KEY_SPACE;
    case KEY_LEFTCTRL:
      return KEY_ESC;
    case KEY_LEFTSHIFT:
      return KEY_LEFTCTRL;
    case KEY_LEFTALT:
      return KEY_LEFTALT;
    case KEY_ENTER:
      return KEY_ENTER;
    case KEY_RIGHTCTRL:
      return KEY_TAB;
    case KEY_ESC:
      return KEY_ESC;
    case KEY_TAB:
      return KEY_F1;
    case KEY_BACKSPACE:
      return KEY_BACKSPACE;
    case KEY_E:
      return KEY_E;
    case KEY_T:
      return KEY_T;
    default:
      return -1;
    }
  }
}

static int handle_keyboard_key(int uinput_fd, const struct config *cfg, int key_code,
                               int pressed) {
  int mapped = map_key_to_keyboard(cfg, key_code);
  if (mapped < 0) {
    return 1;
  }
  return emit_button_state(uinput_fd, mapped, pressed);
}

static int handle_button_key(int uinput_fd, const struct config *cfg, struct button_state *state,
                             int key_code,
                             int pressed) {
  if (cfg->device_mode == DEVICE_MODE_KEYBOARD) {
    return handle_keyboard_key(uinput_fd, cfg, key_code, pressed);
  }

  int mapped = map_key_to_button(cfg, key_code);
  enum shoulder_role shoulder = map_key_to_shoulder_role(cfg, key_code);
  int old_value;
  int new_value;

  if (mapped >= 0) {
    return emit_button_state(uinput_fd, mapped, pressed);
  }

  if (cfg->trigger_mode == TRIGGER_MODE_AXES) {
    switch (shoulder) {
    case SHOULDER_ROLE_L2:
      old_value = state->lt;
      state->lt = pressed ? TRIGGER_PRESSED : TRIGGER_RELEASED;
      return old_value == state->lt || emit_abs_state(uinput_fd, ABS_Z, state->lt);
    case SHOULDER_ROLE_R2:
      old_value = state->rt;
      state->rt = pressed ? TRIGGER_PRESSED : TRIGGER_RELEASED;
      return old_value == state->rt || emit_abs_state(uinput_fd, ABS_RZ, state->rt);
    default:
      break;
    }
  }

  switch (key_code) {
  case KEY_UP:
    state->up = pressed;
    old_value = state->hat_y;
    new_value = button_hat_value(state->up, state->down);
    state->hat_y = new_value;
    return old_value == new_value || emit_abs_state(uinput_fd, ABS_HAT0Y, new_value);
  case KEY_DOWN:
    state->down = pressed;
    old_value = state->hat_y;
    new_value = button_hat_value(state->up, state->down);
    state->hat_y = new_value;
    return old_value == new_value || emit_abs_state(uinput_fd, ABS_HAT0Y, new_value);
  case KEY_LEFT:
    state->left = pressed;
    old_value = state->hat_x;
    new_value = button_hat_value(state->left, state->right);
    state->hat_x = new_value;
    return old_value == new_value || emit_abs_state(uinput_fd, ABS_HAT0X, new_value);
  case KEY_RIGHT:
    state->right = pressed;
    old_value = state->hat_x;
    new_value = button_hat_value(state->left, state->right);
    state->hat_x = new_value;
    return old_value == new_value || emit_abs_state(uinput_fd, ABS_HAT0X, new_value);
  default:
    return 1;
  }
}

static int read_button_events(int button_fd, int uinput_fd, const struct config *cfg,
                              struct button_state *state,
                              int *button_events_out) {
  int handled = 0;

  for (;;) {
    struct input_event ev;
    ssize_t n = read(button_fd, &ev, sizeof(ev));
    if (n == (ssize_t)sizeof(ev)) {
      if (ev.type == EV_KEY && (ev.value == 0 || ev.value == 1)) {
        int pressed = ev.value != 0;
        if (handle_button_key(uinput_fd, cfg, state, ev.code, pressed)) {
          handled++;
          (*button_events_out)++;
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
  return handled;
}

static void usage(const char *argv0) {
  printf("Usage: %s [options]\n", argv0);
  printf("Options:\n");
  printf("  --serial PATH          Serial raw stick path. Default: %s\n", DEFAULT_SERIAL_PATH);
  printf("  --calibration PATH     Calibration path. Default: %s\n", DEFAULT_CALIBRATION_PATH);
  printf("  --uinput PATH          uinput path. Default: %s\n", DEFAULT_UINPUT_PATH);
  printf("  --device-mode MODE     analog or xbox. Default: analog.\n");
  printf("  --trigger-mode MODE    buttons or axes for L2/R2 in xbox mode. Default: buttons.\n");
  printf("  --shoulder-layout MODE standard or user. Default: standard.\n");
  printf("  --keyboard-profile NAME passthrough, dosbox, or digger. Default: dosbox.\n");
  printf("  --button-event PATH    Button event source for xbox mode. Default: %s.\n",
         DEFAULT_BUTTON_EVENT_PATH);
  printf("  --no-buttons           Do not forward physical buttons in xbox mode.\n");
  printf("  --timeout-ms MS        Stop after MS. Default: 0, run until signal.\n");
  printf("  --no-uinput            Do not create a virtual input device.\n");
  printf("  --x-source NAME        axisYL, axisXL, axisYR, or axisXR. Default: axisXR.\n");
  printf("  --y-source NAME        axisYL, axisXL, axisYR, or axisXR. Default: axisYR.\n");
  printf("  --invert-x             Invert normalized ABS_X.\n");
  printf("  --invert-y             Invert normalized ABS_Y.\n");
  printf("  --function-button NAME mode, thumbl, thumbr, or none. Default: mode.\n");
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
  int button_fd = -1;
  int uinput_fd = -1;
  long long start_ms;
  long long deadline = 0;
  unsigned char window[6];
  int window_count = 0;
  int frames = 0;
  int emitted = 0;
  int button_events = 0;
  int last_x = 999999;
  int last_y = 999999;
  struct button_state button_state;

  cfg.serial_path = DEFAULT_SERIAL_PATH;
  cfg.calibration_path = DEFAULT_CALIBRATION_PATH;
  cfg.uinput_path = DEFAULT_UINPUT_PATH;
  cfg.button_event_path = DEFAULT_BUTTON_EVENT_PATH;
  cfg.device_mode = DEVICE_MODE_ANALOG;
  cfg.trigger_mode = TRIGGER_MODE_BUTTONS;
  cfg.shoulder_layout = SHOULDER_LAYOUT_STANDARD;
  cfg.keyboard_profile = KEYBOARD_PROFILE_DOSBOX;
  cfg.baud = DEFAULT_BAUD;
  cfg.timeout_ms = 0;
  cfg.no_uinput = 0;
  cfg.no_buttons = 0;
  cfg.verbose = 0;
  cfg.print_every = 0;
  cfg.deadzone_raw = DEFAULT_DEADZONE_RAW;
  cfg.x_source = AXIS_XR;
  cfg.y_source = AXIS_YR;
  cfg.invert_x = 0;
  cfg.invert_y = 0;
  cfg.function_button_code = BTN_MODE;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--serial") == 0 && i + 1 < argc) {
      cfg.serial_path = argv[++i];
    } else if (strcmp(argv[i], "--calibration") == 0 && i + 1 < argc) {
      cfg.calibration_path = argv[++i];
    } else if (strcmp(argv[i], "--uinput") == 0 && i + 1 < argc) {
      cfg.uinput_path = argv[++i];
    } else if (strcmp(argv[i], "--device-mode") == 0 && i + 1 < argc) {
      if (!parse_device_mode(argv[++i], &cfg.device_mode)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--trigger-mode") == 0 && i + 1 < argc) {
      if (!parse_trigger_mode(argv[++i], &cfg.trigger_mode)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--shoulder-layout") == 0 && i + 1 < argc) {
      if (!parse_shoulder_layout(argv[++i], &cfg.shoulder_layout)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--keyboard-profile") == 0 && i + 1 < argc) {
      if (!parse_keyboard_profile(argv[++i], &cfg.keyboard_profile)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--button-event") == 0 && i + 1 < argc) {
      cfg.button_event_path = argv[++i];
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
    } else if (strcmp(argv[i], "--function-button") == 0 && i + 1 < argc) {
      if (!parse_function_button(argv[++i], &cfg.function_button_code)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--no-uinput") == 0) {
      cfg.no_uinput = 1;
    } else if (strcmp(argv[i], "--no-buttons") == 0) {
      cfg.no_buttons = 1;
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
  printf("serial=%s baud=%d calibration=%s uinput=%s device_mode=%s trigger_mode=%s shoulder_layout=%s keyboard_profile=%s button_event=%s no_uinput=%s no_buttons=%s x_source=%s y_source=%s invert_x=%s invert_y=%s function_button=%s deadzone_raw=%d timeout_ms=%d\n",
         cfg.serial_path, cfg.baud, cfg.calibration_path, cfg.uinput_path,
         device_mode_name(cfg.device_mode), trigger_mode_name(cfg.trigger_mode),
         shoulder_layout_name(cfg.shoulder_layout), keyboard_profile_name(cfg.keyboard_profile),
         cfg.button_event_path,
         cfg.no_uinput ? "yes" : "no", cfg.no_buttons ? "yes" : "no",
         axis_source_name(cfg.x_source),
         axis_source_name(cfg.y_source), cfg.invert_x ? "yes" : "no",
         cfg.invert_y ? "yes" : "no", function_button_name(cfg.function_button_code),
         cfg.deadzone_raw, cfg.timeout_ms);

  load_calibration(cfg.calibration_path, &cal);
  serial_fd = open_serial(cfg.serial_path, cfg.baud, &old_tio, &have_old_tio);
  if (serial_fd < 0) {
    return 1;
  }

  if (!cfg.no_uinput) {
    uinput_fd = create_uinput_device(&cfg);
    if (uinput_fd < 0) {
      if (have_old_tio) {
        tcsetattr(serial_fd, TCSANOW, &old_tio);
      }
      close(serial_fd);
      return 1;
    }
    if (cfg.device_mode == DEVICE_MODE_XBOX || cfg.device_mode == DEVICE_MODE_KEYBOARD) {
      memset(&button_state, 0, sizeof(button_state));
      button_state.lt = TRIGGER_RELEASED;
      button_state.rt = TRIGGER_RELEASED;
      if (cfg.device_mode == DEVICE_MODE_XBOX) {
        emit_xbox_idle_state(uinput_fd, cfg.trigger_mode);
      }
      if (!cfg.no_buttons) {
        button_fd = open_button_event(cfg.button_event_path);
      }
    }
  }

  memset(window, 0, sizeof(window));
  start_ms = now_ms();
  if (cfg.timeout_ms > 0) {
    deadline = start_ms + cfg.timeout_ms;
  }

  while (!g_stop) {
    struct pollfd pfds[2];
    nfds_t nfds = 0;
    int pr;
    struct joy_frame frame;

    if (cfg.timeout_ms > 0 && now_ms() >= deadline) {
      break;
    }

    pfds[nfds].fd = serial_fd;
    pfds[nfds].events = POLLIN;
    pfds[nfds].revents = 0;
    nfds++;
    if (button_fd >= 0) {
      pfds[nfds].fd = button_fd;
      pfds[nfds].events = POLLIN;
      pfds[nfds].revents = 0;
      nfds++;
    }

    pr = poll(pfds, nfds, 100);
    if (pr < 0) {
      if (errno == EINTR) {
        continue;
      }
      printf("serial path=%s poll=no errno=%d %s\n", cfg.serial_path, errno,
             strerror(errno));
      break;
    }
    if (pr == 0) {
      continue;
    }

    memset(&frame, 0, sizeof(frame));
    if ((pfds[0].revents & POLLIN) &&
        read_frames(serial_fd, window, &window_count, &frame, &frames)) {
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
    if (button_fd >= 0 && nfds > 1 && (pfds[1].revents & POLLIN)) {
      read_button_events(button_fd, uinput_fd, &cfg, &button_state, &button_events);
    }
  }

  if (have_old_tio) {
    tcsetattr(serial_fd, TCSANOW, &old_tio);
  }
  if (button_fd >= 0) {
    close(button_fd);
  }
  close(serial_fd);
  destroy_uinput_device(uinput_fd);
  printf("summary frames=%d emitted=%d button_events=%d last_x=%d last_y=%d duration_ms=%lld\n",
         frames, emitted, button_events, last_x == 999999 ? 0 : last_x,
         last_y == 999999 ? 0 : last_y, now_ms() - start_ms);
  return 0;
}
