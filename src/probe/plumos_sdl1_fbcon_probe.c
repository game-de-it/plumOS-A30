#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <SDL.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int signum) {
  (void)signum;
  g_stop = 1;
}

struct options {
  int width;
  int height;
  int bpp;
  int run_ms;
  int fullscreen;
  int doublebuf;
  int hwsurface;
  int update_flip;
  int fps;
};

static void usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [--mode WxH] [--bpp N] [--run-ms N] [--fps N]\n"
          "          [--fullscreen 0|1] [--doublebuf 0|1] [--hwsurface 0|1]\n"
          "          [--update rect|flip]\n",
          argv0);
}

static int parse_bool(const char *value, int *out) {
  if (strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
      strcmp(value, "yes") == 0 || strcmp(value, "on") == 0) {
    *out = 1;
    return 1;
  }
  if (strcmp(value, "0") == 0 || strcmp(value, "false") == 0 ||
      strcmp(value, "no") == 0 || strcmp(value, "off") == 0) {
    *out = 0;
    return 1;
  }
  return 0;
}

static int parse_mode(const char *value, int *width, int *height) {
  char *end = NULL;
  long w = strtol(value, &end, 10);
  if (end == value || (*end != 'x' && *end != 'X')) {
    return 0;
  }
  const char *h_text = end + 1;
  long h = strtol(h_text, &end, 10);
  if (end == h_text || *end != '\0' || w <= 0 || h <= 0 || w > 4096 || h > 4096) {
    return 0;
  }
  *width = (int)w;
  *height = (int)h;
  return 1;
}

static int parse_args(int argc, char **argv, struct options *opts) {
  opts->width = 640;
  opts->height = 480;
  opts->bpp = 16;
  opts->run_ms = 15000;
  opts->fullscreen = 1;
  opts->doublebuf = 0;
  opts->hwsurface = 0;
  opts->update_flip = 0;
  opts->fps = 30;

  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    const char *value = NULL;
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      usage(argv[0]);
      exit(0);
    } else if (strcmp(arg, "--mode") == 0 && i + 1 < argc) {
      value = argv[++i];
      if (!parse_mode(value, &opts->width, &opts->height)) {
        fprintf(stderr, "error: invalid mode: %s\n", value);
        return 0;
      }
    } else if (strcmp(arg, "--bpp") == 0 && i + 1 < argc) {
      opts->bpp = atoi(argv[++i]);
    } else if (strcmp(arg, "--run-ms") == 0 && i + 1 < argc) {
      opts->run_ms = atoi(argv[++i]);
    } else if (strcmp(arg, "--fps") == 0 && i + 1 < argc) {
      opts->fps = atoi(argv[++i]);
    } else if (strcmp(arg, "--fullscreen") == 0 && i + 1 < argc) {
      if (!parse_bool(argv[++i], &opts->fullscreen)) {
        fprintf(stderr, "error: invalid fullscreen value\n");
        return 0;
      }
    } else if (strcmp(arg, "--doublebuf") == 0 && i + 1 < argc) {
      if (!parse_bool(argv[++i], &opts->doublebuf)) {
        fprintf(stderr, "error: invalid doublebuf value\n");
        return 0;
      }
    } else if (strcmp(arg, "--hwsurface") == 0 && i + 1 < argc) {
      if (!parse_bool(argv[++i], &opts->hwsurface)) {
        fprintf(stderr, "error: invalid hwsurface value\n");
        return 0;
      }
    } else if (strcmp(arg, "--update") == 0 && i + 1 < argc) {
      value = argv[++i];
      if (strcmp(value, "rect") == 0) {
        opts->update_flip = 0;
      } else if (strcmp(value, "flip") == 0) {
        opts->update_flip = 1;
      } else {
        fprintf(stderr, "error: invalid update method: %s\n", value);
        return 0;
      }
    } else {
      fprintf(stderr, "error: unknown argument: %s\n", arg);
      usage(argv[0]);
      return 0;
    }
  }

  if (opts->bpp != 16 && opts->bpp != 24 && opts->bpp != 32) {
    fprintf(stderr, "error: unsupported bpp: %d\n", opts->bpp);
    return 0;
  }
  if (opts->run_ms <= 0) {
    opts->run_ms = 15000;
  }
  if (opts->fps <= 0 || opts->fps > 120) {
    opts->fps = 30;
  }
  return 1;
}

static uint32_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

static void print_fb_info(const char *label) {
  int fd = open("/dev/fb0", O_RDONLY);
  if (fd < 0) {
    printf("%s fb open=no errno=%d\n", label, errno);
    return;
  }
  struct fb_var_screeninfo vinfo;
  struct fb_fix_screeninfo finfo;
  memset(&vinfo, 0, sizeof(vinfo));
  memset(&finfo, 0, sizeof(finfo));
  if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == 0 &&
      ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == 0) {
    printf("%s fb xres=%u yres=%u virtual=%ux%u offset=%u,%u bpp=%u line=%u "
           "rgba=%u/%u,%u/%u,%u/%u,%u/%u\n",
           label, vinfo.xres, vinfo.yres, vinfo.xres_virtual, vinfo.yres_virtual,
           vinfo.xoffset, vinfo.yoffset, vinfo.bits_per_pixel, finfo.line_length,
           vinfo.red.offset, vinfo.red.length, vinfo.green.offset, vinfo.green.length,
           vinfo.blue.offset, vinfo.blue.length, vinfo.transp.offset,
           vinfo.transp.length);
  } else {
    printf("%s fb ioctl=no errno=%d\n", label, errno);
  }
  close(fd);
}

static uint32_t map_rgb(SDL_Surface *surface, uint8_t r, uint8_t g, uint8_t b) {
  return SDL_MapRGB(surface->format, r, g, b);
}

static void put_pixel(SDL_Surface *surface, int x, int y, uint32_t color) {
  if (x < 0 || y < 0 || x >= surface->w || y >= surface->h) {
    return;
  }
  uint8_t *base = (uint8_t *)surface->pixels + y * surface->pitch +
                  x * surface->format->BytesPerPixel;
  switch (surface->format->BytesPerPixel) {
    case 2:
      *(uint16_t *)base = (uint16_t)color;
      break;
    case 3:
      if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
        base[0] = (uint8_t)((color >> 16) & 0xff);
        base[1] = (uint8_t)((color >> 8) & 0xff);
        base[2] = (uint8_t)(color & 0xff);
      } else {
        base[0] = (uint8_t)(color & 0xff);
        base[1] = (uint8_t)((color >> 8) & 0xff);
        base[2] = (uint8_t)((color >> 16) & 0xff);
      }
      break;
    case 4:
      *(uint32_t *)base = color;
      break;
    default:
      *(uint8_t *)base = (uint8_t)color;
      break;
  }
}

static void fill_rect(SDL_Surface *surface, int x, int y, int w, int h, uint32_t color) {
  if (w <= 0 || h <= 0) {
    return;
  }
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > surface->w) {
    w = surface->w - x;
  }
  if (y + h > surface->h) {
    h = surface->h - y;
  }
  if (w <= 0 || h <= 0) {
    return;
  }
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) {
      put_pixel(surface, xx, yy, color);
    }
  }
}

static const uint8_t *glyph_rows(char ch) {
  static const uint8_t space[7] = {0, 0, 0, 0, 0, 0, 0};
  static const uint8_t unknown[7] = {0x1f, 0x11, 0x01, 0x06, 0x04, 0, 0x04};
  static const uint8_t glyphs[][7] = {
      {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}, /* 0 */
      {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e}, /* 1 */
      {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}, /* 2 */
      {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e}, /* 3 */
      {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}, /* 4 */
      {0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e}, /* 5 */
      {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e}, /* 6 */
      {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
      {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}, /* 8 */
      {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c}, /* 9 */
      {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}, /* A */
      {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}, /* B */
      {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e}, /* C */
      {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e}, /* D */
      {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}, /* E */
      {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}, /* F */
      {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f}, /* G */
      {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}, /* H */
      {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}, /* I */
      {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c}, /* J */
      {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* K */
      {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}, /* L */
      {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}, /* M */
      {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* N */
      {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, /* O */
      {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}, /* P */
      {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}, /* Q */
      {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}, /* R */
      {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}, /* S */
      {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
      {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, /* U */
      {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}, /* V */
      {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}, /* W */
      {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}, /* X */
      {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04}, /* Y */
      {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}, /* Z */
      {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00}, /* - */
      {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00}, /* : */
  };
  if (ch >= '0' && ch <= '9') {
    return glyphs[ch - '0'];
  }
  if (ch >= 'a' && ch <= 'z') {
    ch = (char)(ch - 'a' + 'A');
  }
  if (ch >= 'A' && ch <= 'Z') {
    return glyphs[10 + ch - 'A'];
  }
  if (ch == '-') {
    return glyphs[36];
  }
  if (ch == ':') {
    return glyphs[37];
  }
  if (ch == ' ') {
    return space;
  }
  return unknown;
}

static int text_width(const char *text, int scale) {
  return (int)strlen(text) * 6 * scale;
}

static void draw_text(SDL_Surface *surface, int x, int y, const char *text, int scale,
                      uint32_t color) {
  for (const char *p = text; *p; p++, x += 6 * scale) {
    const uint8_t *rows = glyph_rows(*p);
    for (int yy = 0; yy < 7; yy++) {
      for (int xx = 0; xx < 5; xx++) {
        if (rows[yy] & (1u << (4 - xx))) {
          fill_rect(surface, x + xx * scale, y + yy * scale, scale, scale, color);
        }
      }
    }
  }
}

static void draw_line(SDL_Surface *surface, int x0, int y0, int x1, int y1, int thickness,
                      uint32_t color) {
  int dx = abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    fill_rect(surface, x0 - thickness / 2, y0 - thickness / 2, thickness, thickness, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

static void draw_pattern(SDL_Surface *surface, const struct options *opts, int frame) {
  const uint32_t black = map_rgb(surface, 0, 0, 0);
  const uint32_t white = map_rgb(surface, 255, 255, 255);
  const uint32_t red = map_rgb(surface, 255, 32, 32);
  const uint32_t green = map_rgb(surface, 32, 220, 48);
  const uint32_t blue = map_rgb(surface, 48, 96, 255);
  const uint32_t yellow = map_rgb(surface, 255, 230, 32);
  const uint32_t cyan = map_rgb(surface, 32, 230, 230);
  const uint32_t magenta = map_rgb(surface, 220, 64, 220);
  const uint32_t orange = map_rgb(surface, 255, 140, 32);
  const uint32_t gray = map_rgb(surface, 48, 48, 56);

  fill_rect(surface, 0, 0, surface->w, surface->h, black);

  int band = surface->w < surface->h ? surface->w / 18 : surface->h / 18;
  if (band < 8) {
    band = 8;
  }
  fill_rect(surface, 0, 0, surface->w, band, red);
  fill_rect(surface, 0, surface->h - band, surface->w, band, yellow);
  fill_rect(surface, 0, 0, band, surface->h, green);
  fill_rect(surface, surface->w - band, 0, band, surface->h, blue);

  int inner_x = band + 6;
  int inner_y = band + 6;
  int inner_w = surface->w - 2 * inner_x;
  int inner_h = surface->h - 2 * inner_y;
  int bars = 12;
  for (int i = 0; i < bars; i++) {
    int x0 = inner_x + i * inner_w / bars;
    int x1 = inner_x + (i + 1) * inner_w / bars;
    uint32_t color;
    switch (i % 6) {
      case 0: color = red; break;
      case 1: color = green; break;
      case 2: color = blue; break;
      case 3: color = yellow; break;
      case 4: color = cyan; break;
      default: color = magenta; break;
    }
    fill_rect(surface, x0, inner_y, x1 - x0, inner_h, color);
  }
  fill_rect(surface, inner_x + inner_w / 6, inner_y + inner_h / 5,
            inner_w * 2 / 3, inner_h * 3 / 5, gray);

  draw_line(surface, inner_x, inner_y, inner_x + inner_w - 1, inner_y + inner_h - 1, 5, white);
  draw_line(surface, inner_x + inner_w - 1, inner_y, inner_x, inner_y + inner_h - 1, 5, cyan);

  int box = surface->w < surface->h ? surface->w / 7 : surface->h / 7;
  if (box < 36) {
    box = 36;
  }
  fill_rect(surface, band + 12, band + 12, box, box, red);
  fill_rect(surface, surface->w - band - 12 - box, band + 12, box, box, green);
  fill_rect(surface, band + 12, surface->h - band - 12 - box, box, box, blue);
  fill_rect(surface, surface->w - band - 12 - box, surface->h - band - 12 - box, box, box,
            yellow);

  int scale = surface->w >= 600 ? 4 : 3;
  char title[96];
  snprintf(title, sizeof(title), "SDL1 %dX%d BPP%d", opts->width, opts->height, opts->bpp);
  draw_text(surface, (surface->w - text_width(title, scale)) / 2,
            surface->h / 2 - 38 * scale / 4, title, scale, white);
  snprintf(title, sizeof(title), "%s FRAME %04d", opts->update_flip ? "FLIP" : "RECT", frame);
  draw_text(surface, (surface->w - text_width(title, scale)) / 2,
            surface->h / 2 + 4 * scale, title, scale, orange);

  draw_text(surface, band + 18, band + 22, "TL", scale, white);
  draw_text(surface, surface->w - band - 18 - text_width("TR", scale), band + 22, "TR",
            scale, white);
  draw_text(surface, band + 18, surface->h - band - 22 - 7 * scale, "BL", scale, white);
  draw_text(surface, surface->w - band - 18 - text_width("BR", scale),
            surface->h - band - 22 - 7 * scale, "BR", scale, black);

  int marker_size = box / 2;
  int travel_w = surface->w - 2 * band - marker_size;
  int mx = band + (frame * 9) % (travel_w > 1 ? travel_w : 1);
  int my = surface->h / 2 - marker_size / 2;
  fill_rect(surface, mx, my, marker_size, marker_size, orange);
}

int main(int argc, char **argv) {
  struct options opts;
  if (!parse_args(argc, argv, &opts)) {
    return 2;
  }

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  SDL_version compiled;
  const SDL_version *linked = SDL_Linked_Version();
  SDL_VERSION(&compiled);
  printf("compiled_sdl=%u.%u.%u linked_sdl=%u.%u.%u\n", compiled.major, compiled.minor,
         compiled.patch, linked->major, linked->minor, linked->patch);
  printf("request mode=%dx%d bpp=%d fullscreen=%d hwsurface=%d doublebuf=%d update=%s "
         "run_ms=%d\n",
         opts.width, opts.height, opts.bpp, opts.fullscreen, opts.hwsurface, opts.doublebuf,
         opts.update_flip ? "flip" : "rect", opts.run_ms);
  print_fb_info("before");

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "sdl init=no error=\"%s\"\n", SDL_GetError());
    return 1;
  }

  char driver[64];
  if (SDL_VideoDriverName(driver, sizeof(driver))) {
    printf("video_driver=%s\n", driver);
  } else {
    printf("video_driver=unknown\n");
  }

  uint32_t flags = opts.hwsurface ? SDL_HWSURFACE : SDL_SWSURFACE;
  if (opts.fullscreen) {
    flags |= SDL_FULLSCREEN;
  }
  if (opts.doublebuf) {
    flags |= SDL_DOUBLEBUF;
  }

  SDL_Surface *screen = SDL_SetVideoMode(opts.width, opts.height, opts.bpp, flags);
  if (!screen) {
    fprintf(stderr, "set_video_mode=no error=\"%s\"\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  printf("surface w=%d h=%d pitch=%d bytespp=%u flags=0x%x masks=%08x,%08x,%08x,%08x\n",
         screen->w, screen->h, screen->pitch, screen->format->BytesPerPixel, screen->flags,
         screen->format->Rmask, screen->format->Gmask, screen->format->Bmask,
         screen->format->Amask);
  print_fb_info("after_set_video");
  fflush(stdout);

  const uint32_t start = now_ms();
  uint32_t next_frame = start;
  int frame = 0;
  while (!g_stop && (int)(now_ms() - start) < opts.run_ms) {
    if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) != 0) {
      fprintf(stderr, "lock=no error=\"%s\"\n", SDL_GetError());
      break;
    }
    draw_pattern(screen, &opts, frame);
    if (SDL_MUSTLOCK(screen)) {
      SDL_UnlockSurface(screen);
    }
    if (opts.update_flip) {
      SDL_Flip(screen);
    } else {
      SDL_UpdateRect(screen, 0, 0, 0, 0);
    }
    frame++;
    next_frame += (uint32_t)(1000 / opts.fps);
    uint32_t now = now_ms();
    if ((int32_t)(next_frame - now) > 0) {
      SDL_Delay(next_frame - now);
    } else {
      next_frame = now;
    }
  }

  printf("frames=%d elapsed_ms=%u result=ok\n", frame, now_ms() - start);
  fflush(stdout);
  SDL_Quit();
  print_fb_info("after_quit");
  return 0;
}
