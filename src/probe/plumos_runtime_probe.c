#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<linux/fb.h>)
#include <linux/fb.h>
#define PLUMOS_HAS_LINUX_FB 1
#endif
#if __has_include(<linux/input.h>)
#include <linux/input.h>
#define PLUMOS_HAS_LINUX_INPUT 1
#endif
#if __has_include(<sys/soundcard.h>)
#include <sys/soundcard.h>
#define PLUMOS_HAS_OSS_AUDIO 1
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

struct probe_config {
  const char *fb_path;
  const char *input_path;
  const char *dsp_path;
  int do_video;
  int do_input;
  int do_audio;
  int draw_ms;
  int input_ms;
  int audio_ms;
  int allow_busy_audio;
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

static long long now_ms(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    return 0;
  }
  return (long long)tv.tv_sec * 1000LL + (long long)tv.tv_usec / 1000LL;
}

#if defined(PLUMOS_HAS_LINUX_FB)
static uint32_t test_color(int x, int y) {
  if (((x / 8) + (y / 8)) & 1) {
    return 0x00ff3b30u;
  }
  return 0x0028c76fu;
}

static void put_fb_pixel(unsigned char *base, long offset, int bpp, uint32_t rgb) {
  unsigned char *p = base + offset;

  if (bpp == 32) {
    p[0] = (unsigned char)(rgb & 0xff);
    p[1] = (unsigned char)((rgb >> 8) & 0xff);
    p[2] = (unsigned char)((rgb >> 16) & 0xff);
    p[3] = 0;
  } else if (bpp == 24) {
    p[0] = (unsigned char)(rgb & 0xff);
    p[1] = (unsigned char)((rgb >> 8) & 0xff);
    p[2] = (unsigned char)((rgb >> 16) & 0xff);
  } else if (bpp == 16) {
    uint16_t r = (uint16_t)((rgb >> 19) & 0x1f);
    uint16_t g = (uint16_t)((rgb >> 10) & 0x3f);
    uint16_t b = (uint16_t)((rgb >> 3) & 0x1f);
    uint16_t v = (uint16_t)((r << 11) | (g << 5) | b);
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
  }
}
#endif

static int run_video_probe(const struct probe_config *cfg) {
#if defined(PLUMOS_HAS_LINUX_FB)
  int fd;
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
  long map_size;
  int draw_w;
  int draw_h;
  int bytes_per_pixel;
  int ok = 1;

  fd = open(cfg->fb_path, cfg->draw_ms > 0 ? O_RDWR : O_RDONLY);
  if (fd < 0) {
    printf("video path=%s open=no errno=%d %s\n", cfg->fb_path, errno, strerror(errno));
    return 0;
  }
  memset(&var, 0, sizeof(var));
  memset(&fix, 0, sizeof(fix));
  if (ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0 ||
      ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0) {
    printf("video path=%s open=yes ioctl=no errno=%d %s\n", cfg->fb_path, errno,
           strerror(errno));
    close(fd);
    return 0;
  }

  printf("video path=%s open=yes xres=%u yres=%u virtual=%ux%u offset=%u,%u bpp=%u line=%u\n",
         cfg->fb_path, var.xres, var.yres, var.xres_virtual, var.yres_virtual,
         var.xoffset, var.yoffset, var.bits_per_pixel, fix.line_length);

  bytes_per_pixel = (int)((var.bits_per_pixel + 7) / 8);
  draw_w = var.xres < 64 ? (int)var.xres : 64;
  draw_h = var.yres < 64 ? (int)var.yres : 64;

  if (cfg->draw_ms <= 0) {
    printf("video draw=skipped\n");
    close(fd);
    return 1;
  }
  if (!(var.bits_per_pixel == 16 || var.bits_per_pixel == 24 || var.bits_per_pixel == 32) ||
      fix.line_length == 0 || bytes_per_pixel <= 0 || draw_w <= 0 || draw_h <= 0) {
    printf("video draw=no reason=unsupported_geometry\n");
    close(fd);
    return 0;
  }
  if ((long)var.xoffset + draw_w > (long)var.xres_virtual ||
      (long)var.yoffset + draw_h > (long)var.yres_virtual) {
    printf("video draw=no reason=offset_out_of_bounds\n");
    close(fd);
    return 0;
  }

  map_size = fix.smem_len ? (long)fix.smem_len
                          : (long)fix.line_length * (long)var.yres_virtual;
  if (map_size <= 0) {
    printf("video draw=no reason=invalid_map_size\n");
    close(fd);
    return 0;
  }
  {
    long first_row_offset = (long)var.yoffset * (long)fix.line_length +
                            (long)var.xoffset * (long)bytes_per_pixel;
    long last_row_offset = ((long)var.yoffset + draw_h - 1) * (long)fix.line_length +
                           (long)var.xoffset * (long)bytes_per_pixel;
    long backup_row = (long)draw_w * (long)bytes_per_pixel;
    if (first_row_offset < 0 || last_row_offset < 0 ||
        first_row_offset + backup_row > map_size ||
        last_row_offset + backup_row > map_size) {
      printf("video draw=no reason=row_out_of_bounds\n");
      close(fd);
      return 0;
    }
  }

  {
    unsigned char *fb;
    unsigned char *backup;
    int x;
    int y;
    size_t backup_row = (size_t)draw_w * (size_t)bytes_per_pixel;

    fb = (unsigned char *)mmap(NULL, (size_t)map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
      printf("video draw=no mmap_errno=%d %s\n", errno, strerror(errno));
      close(fd);
      return 0;
    }
    backup = (unsigned char *)malloc(backup_row * (size_t)draw_h);
    if (!backup) {
      printf("video draw=no reason=alloc_failed\n");
      munmap(fb, (size_t)map_size);
      close(fd);
      return 0;
    }

    for (y = 0; y < draw_h; y++) {
      long row_offset = ((long)var.yoffset + y) * (long)fix.line_length +
                        (long)var.xoffset * (long)bytes_per_pixel;
      memcpy(backup + (size_t)y * backup_row, fb + row_offset, backup_row);
      for (x = 0; x < draw_w; x++) {
        long off = row_offset + (long)x * (long)bytes_per_pixel;
        put_fb_pixel(fb, off, (int)var.bits_per_pixel, test_color(x, y));
      }
    }
    msync(fb, (size_t)map_size, MS_SYNC);
    usleep((useconds_t)cfg->draw_ms * 1000u);
    for (y = 0; y < draw_h; y++) {
      long row_offset = ((long)var.yoffset + y) * (long)fix.line_length +
                        (long)var.xoffset * (long)bytes_per_pixel;
      memcpy(fb + row_offset, backup + (size_t)y * backup_row, backup_row);
    }
    msync(fb, (size_t)map_size, MS_SYNC);
    free(backup);
    munmap(fb, (size_t)map_size);
  }

  printf("video draw=ok pixels=%dx%d ms=%d\n", draw_w, draw_h, cfg->draw_ms);
  close(fd);
  return ok;
#else
  int fd = open(cfg->fb_path, O_RDONLY);
  if (fd >= 0) {
    close(fd);
    printf("video path=%s open=yes fb_headers=no\n", cfg->fb_path);
    return 1;
  }
  printf("video path=%s open=no fb_headers=no errno=%d %s\n", cfg->fb_path, errno,
         strerror(errno));
  return 0;
#endif
}

static int run_input_probe(const struct probe_config *cfg) {
  int fd;
  struct pollfd pfd;
  long long deadline;
  int events = 0;
  int key_events = 0;

  fd = open(cfg->input_path, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    printf("input path=%s open=no errno=%d %s\n", cfg->input_path, errno, strerror(errno));
    return 0;
  }

  deadline = now_ms() + cfg->input_ms;
  while (now_ms() < deadline) {
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
          printf("input event type=%u code=%u value=%d\n", ev.type, ev.code, ev.value);
        }
      }
    }
  }
  close(fd);
  printf("input path=%s open=yes timeout_ms=%d events=%d key_events=%d\n",
         cfg->input_path, cfg->input_ms, events, key_events);
  return 1;
}

#if defined(PLUMOS_HAS_OSS_AUDIO)
static int write_full(int fd, const unsigned char *buf, size_t size) {
  size_t pos = 0;
  while (pos < size) {
    ssize_t n = write(fd, buf + pos, size - pos);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return 0;
    }
    if (n == 0) {
      return 0;
    }
    pos += (size_t)n;
  }
  return 1;
}
#endif

static int run_audio_probe(const struct probe_config *cfg) {
  int fd;

  fd = open(cfg->dsp_path, O_WRONLY);
  if (fd < 0) {
    if (errno == EBUSY && cfg->allow_busy_audio) {
      printf("audio path=%s open=busy allowed=yes errno=%d %s\n", cfg->dsp_path, errno,
             strerror(errno));
      return 1;
    }
    printf("audio path=%s open=no errno=%d %s\n", cfg->dsp_path, errno, strerror(errno));
    return 0;
  }

#if defined(PLUMOS_HAS_OSS_AUDIO)
  {
    int format = AFMT_S16_LE;
    int channels = 1;
    int speed = 44100;
    int configured = 1;

    if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) != 0) {
      configured = 0;
    }
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) != 0) {
      configured = 0;
    }
    if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) != 0) {
      configured = 0;
    }
    printf("audio path=%s open=yes oss=yes configured=%s format=%d channels=%d speed=%d\n",
           cfg->dsp_path, configured ? "yes" : "no", format, channels, speed);

    if (cfg->audio_ms > 0) {
      int samples = speed * cfg->audio_ms / 1000;
      int period = speed / 440;
      int i;
      unsigned char *buf = (unsigned char *)malloc((size_t)samples * 2u);
      if (!buf) {
        printf("audio tone=no reason=alloc_failed\n");
        close(fd);
        return 0;
      }
      if (period <= 0) {
        period = 1;
      }
      for (i = 0; i < samples; i++) {
        int16_t sample = ((i % period) < (period / 2)) ? 5000 : -5000;
        buf[(size_t)i * 2u] = (unsigned char)(sample & 0xff);
        buf[(size_t)i * 2u + 1u] = (unsigned char)((sample >> 8) & 0xff);
      }
      if (!write_full(fd, buf, (size_t)samples * 2u)) {
        printf("audio tone=no errno=%d %s\n", errno, strerror(errno));
        free(buf);
        close(fd);
        return 0;
      }
      free(buf);
      printf("audio tone=ok ms=%d samples=%d\n", cfg->audio_ms, samples);
    } else {
      printf("audio tone=skipped\n");
    }
  }
#else
  printf("audio path=%s open=yes oss_headers=no\n", cfg->dsp_path);
  printf("audio tone=skipped\n");
#endif

  close(fd);
  return 1;
}

static void usage(const char *argv0) {
  printf("Usage: %s [OPTIONS]\n", argv0);
  printf("\n");
  printf("Options:\n");
  printf("  --fb PATH        Default: $PLUMOS_FB or /dev/fb0\n");
  printf("  --input PATH     Default: $PLUMOS_INPUT_EVENT or auto-detect gpio-keys-polled\n");
  printf("  --dsp PATH       Default: $PLUMOS_DSP or /dev/dsp\n");
  printf("  --draw-ms MS     Draw and restore a small framebuffer patch. Default: 0\n");
  printf("  --input-ms MS    Poll input for this long. Default: 250\n");
  printf("  --audio-ms MS    Write a quiet OSS tone for this long. Default: 0\n");
  printf("  --allow-busy-audio  Treat EBUSY on the audio device as an observed OK state\n");
  printf("  --no-video       Skip framebuffer probe\n");
  printf("  --no-input       Skip input probe\n");
  printf("  --no-audio       Skip audio probe\n");
}

int main(int argc, char **argv) {
  struct probe_config cfg;
  char input_path[PATH_MAX];
  const char *env;
  int ok = 1;
  int i;

  memset(&cfg, 0, sizeof(cfg));
  cfg.fb_path = "/dev/fb0";
  cfg.input_path = input_path;
  cfg.dsp_path = "/dev/dsp";
  cfg.do_video = 1;
  cfg.do_input = 1;
  cfg.do_audio = 1;
  cfg.input_ms = 250;

  discover_input_event(input_path, sizeof(input_path));
  env = getenv("PLUMOS_FB");
  if (env && env[0]) {
    cfg.fb_path = env;
  }
  env = getenv("PLUMOS_INPUT_EVENT");
  if (env && env[0]) {
    copy_string(input_path, sizeof(input_path), env);
  }
  env = getenv("PLUMOS_DSP");
  if (env && env[0]) {
    cfg.dsp_path = env;
  }

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--fb") == 0 && i + 1 < argc) {
      cfg.fb_path = argv[++i];
    } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
      copy_string(input_path, sizeof(input_path), argv[++i]);
    } else if (strcmp(argv[i], "--dsp") == 0 && i + 1 < argc) {
      cfg.dsp_path = argv[++i];
    } else if (strcmp(argv[i], "--draw-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--draw-ms", argv[++i], 0, 5000, &cfg.draw_ms)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--input-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--input-ms", argv[++i], 0, 60000, &cfg.input_ms)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--audio-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--audio-ms", argv[++i], 0, 5000, &cfg.audio_ms)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--allow-busy-audio") == 0) {
      cfg.allow_busy_audio = 1;
    } else if (strcmp(argv[i], "--no-video") == 0) {
      cfg.do_video = 0;
    } else if (strcmp(argv[i], "--no-input") == 0) {
      cfg.do_input = 0;
    } else if (strcmp(argv[i], "--no-audio") == 0) {
      cfg.do_audio = 0;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "error: unknown or incomplete option: %s\n", argv[i]);
      usage(argv[0]);
      return 2;
    }
  }

  printf("plumOS runtime probe\n");
  if (cfg.do_video) {
    ok = run_video_probe(&cfg) && ok;
  } else {
    printf("video skipped\n");
  }
  if (cfg.do_input) {
    ok = run_input_probe(&cfg) && ok;
  } else {
    printf("input skipped\n");
  }
  if (cfg.do_audio) {
    ok = run_audio_probe(&cfg) && ok;
  } else {
    printf("audio skipped\n");
  }
  printf("result=%s\n", ok ? "ok" : "fail");
  return ok ? 0 : 1;
}
