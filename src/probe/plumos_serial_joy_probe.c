#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DEFAULT_PORT "/dev/ttyS0"
#define DEFAULT_BAUD 9600
#define DEFAULT_TIMEOUT_MS 10000
#define MAX_CHUNK 128

#ifndef CRTSCTS
#define CRTSCTS 0
#endif

struct axis_stats {
  int min;
  int max;
  long sum;
  int count;
};

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

static void print_hex(const unsigned char *buf, ssize_t n) {
  for (ssize_t i = 0; i < n; i++) {
    printf("%02x", buf[i]);
    if (i + 1 < n) {
      putchar(' ');
    }
  }
}

static void update_axis_stats(struct axis_stats *stats, int value) {
  if (stats->count == 0) {
    stats->min = value;
    stats->max = value;
  } else {
    if (value < stats->min) {
      stats->min = value;
    }
    if (value > stats->max) {
      stats->max = value;
    }
  }
  stats->sum += value;
  stats->count++;
}

static void print_axis_stats(const char *name, const struct axis_stats *stats) {
  if (stats->count == 0) {
    printf("axis_stats name=%s samples=0\n", name);
    return;
  }
  printf("axis_stats name=%s samples=%d min=%d max=%d avg=%.2f\n", name, stats->count,
         stats->min, stats->max, (double)stats->sum / (double)stats->count);
}

static void observe_frames(const unsigned char *buf, ssize_t n, unsigned char *window,
                           int *window_count, int *frame_count,
                           struct axis_stats stats[4], int print_frames) {
  for (ssize_t i = 0; i < n; i++) {
    if (*window_count < 6) {
      window[*window_count] = buf[i];
      (*window_count)++;
    } else {
      memmove(window, window + 1, 5);
      window[5] = buf[i];
    }

    if (*window_count == 6 && window[0] == 0xff && window[5] == 0xfe) {
      (*frame_count)++;
      update_axis_stats(&stats[0], window[1]);
      update_axis_stats(&stats[1], window[2]);
      update_axis_stats(&stats[2], window[3]);
      update_axis_stats(&stats[3], window[4]);
      if (print_frames) {
        printf("frame index=%d magic=ff axisYL=%u axisXL=%u axisYR=%u axisXR=%u end=fe\n",
               *frame_count, window[1], window[2], window[3], window[4]);
      }
    }
  }
}

static void usage(const char *argv0) {
  printf("Usage: %s [--port PATH] [--baud N] [--timeout-ms MS] [--frames-only] [--stats-only]\n",
         argv0);
  printf("Serial joystick raw probe for the Miyoo A30 stick path investigation.\n");
  printf("Default port is %s and default baud is %d.\n", DEFAULT_PORT, DEFAULT_BAUD);
}

int main(int argc, char **argv) {
  const char *port = DEFAULT_PORT;
  int baud = DEFAULT_BAUD;
  int timeout_ms = DEFAULT_TIMEOUT_MS;
  int frames_only = 0;
  int stats_only = 0;
  speed_t speed;
  int fd;
  struct termios old_tio;
  struct termios new_tio;
  int have_old_tio = 0;
  long long deadline;
  int bytes_total = 0;
  int reads = 0;
  int frames = 0;
  unsigned char window[6];
  int window_count = 0;
  struct axis_stats stats[4];

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      port = argv[++i];
    } else if (strcmp(argv[i], "--baud") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--baud", argv[++i], 1200, 115200, &baud)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--timeout-ms", argv[++i], 0, 300000, &timeout_ms)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--frames-only") == 0) {
      frames_only = 1;
    } else if (strcmp(argv[i], "--stats-only") == 0) {
      stats_only = 1;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  speed = baud_to_constant(baud);
  if (speed == (speed_t)0) {
    fprintf(stderr, "error: unsupported baud: %d\n", baud);
    return 2;
  }

  printf("plumOS serial joy probe\n");
  printf("port=%s baud=%d timeout_ms=%d frames_only=%s stats_only=%s\n", port, baud,
         timeout_ms, frames_only ? "yes" : "no", stats_only ? "yes" : "no");

  fd = open(port, O_RDONLY | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    printf("serial path=%s open=no errno=%d %s\n", port, errno, strerror(errno));
    return 1;
  }

  if (tcgetattr(fd, &old_tio) == 0) {
    have_old_tio = 1;
    new_tio = old_tio;
    make_raw_8n1(&new_tio, speed);
    if (tcsetattr(fd, TCSANOW, &new_tio) != 0) {
      printf("serial path=%s tcsetattr=no errno=%d %s\n", port, errno, strerror(errno));
    } else {
      printf("serial path=%s tcsetattr=yes raw_8n1=yes\n", port);
    }
  } else {
    printf("serial path=%s tcgetattr=no errno=%d %s\n", port, errno, strerror(errno));
  }

  memset(window, 0, sizeof(window));
  memset(stats, 0, sizeof(stats));
  deadline = now_ms() + timeout_ms;
  while (now_ms() < deadline) {
    struct pollfd pfd;
    int poll_ms = 100;
    int pr;

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pr = poll(&pfd, 1, poll_ms);
    if (pr < 0) {
      if (errno == EINTR) {
        continue;
      }
      printf("serial path=%s poll=no errno=%d %s\n", port, errno, strerror(errno));
      break;
    }
    if (pr == 0 || !(pfd.revents & POLLIN)) {
      continue;
    }
    for (;;) {
      unsigned char buf[MAX_CHUNK];
      ssize_t n = read(fd, buf, sizeof(buf));
      if (n > 0) {
        reads++;
        bytes_total += (int)n;
        if (!frames_only && !stats_only) {
          printf("raw read=%d bytes=%zd data=", reads, n);
          print_hex(buf, n);
          printf("\n");
        }
        observe_frames(buf, n, window, &window_count, &frames, stats, !stats_only);
      } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        break;
      } else if (n < 0 && errno == EINTR) {
        continue;
      } else if (n == 0) {
        break;
      } else {
        printf("serial path=%s read=no errno=%d %s\n", port, errno, strerror(errno));
        break;
      }
    }
    fflush(stdout);
  }

  if (have_old_tio) {
    tcsetattr(fd, TCSANOW, &old_tio);
  }
  close(fd);
  printf("summary path=%s timeout_ms=%d reads=%d bytes=%d frames=%d\n", port, timeout_ms,
         reads, bytes_total, frames);
  print_axis_stats("axisYL", &stats[0]);
  print_axis_stats("axisXL", &stats[1]);
  print_axis_stats("axisYR", &stats[2]);
  print_axis_stats("axisXR", &stats[3]);
  return 0;
}
