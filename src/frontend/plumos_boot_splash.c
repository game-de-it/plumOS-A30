#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <linux/fb.h>

struct fb_ctx {
  int fd;
  uint8_t *mem;
  size_t mem_len;
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
  int logical_w;
  int logical_h;
  int rotate_ccw;
};

static uint32_t pack_color(const struct fb_ctx *ctx, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t pixel = 0;
  uint32_t a = 255;

  if (ctx->var.red.length) {
    pixel |= ((uint32_t)(r >> (8 - ctx->var.red.length))) << ctx->var.red.offset;
  }
  if (ctx->var.green.length) {
    pixel |= ((uint32_t)(g >> (8 - ctx->var.green.length))) << ctx->var.green.offset;
  }
  if (ctx->var.blue.length) {
    pixel |= ((uint32_t)(b >> (8 - ctx->var.blue.length))) << ctx->var.blue.offset;
  }
  if (ctx->var.transp.length) {
    pixel |= ((uint32_t)(a >> (8 - ctx->var.transp.length))) << ctx->var.transp.offset;
  }
  return pixel;
}

static void put_fb_pixel(struct fb_ctx *ctx, int fx, int fy, uint32_t color) {
  int bytes_per_pixel;
  size_t offset;

  if (fx < 0 || fy < 0 || fx >= (int)ctx->var.xres || fy >= (int)ctx->var.yres) {
    return;
  }
  bytes_per_pixel = (int)ctx->var.bits_per_pixel / 8;
  if (bytes_per_pixel < 2) {
    return;
  }
  offset = (size_t)fy * ctx->fix.line_length + (size_t)fx * (size_t)bytes_per_pixel;
  if (offset + (size_t)bytes_per_pixel > ctx->mem_len) {
    return;
  }
  if (bytes_per_pixel == 2) {
    *(uint16_t *)(ctx->mem + offset) = (uint16_t)color;
  } else {
    *(uint32_t *)(ctx->mem + offset) = color;
  }
}

static void put_pixel(struct fb_ctx *ctx, int x, int y, uint32_t color) {
  int fx = x;
  int fy = y;

  if (x < 0 || y < 0 || x >= ctx->logical_w || y >= ctx->logical_h) {
    return;
  }
  if (ctx->rotate_ccw) {
    fx = y;
    fy = (int)ctx->var.yres - 1 - x;
  }
  put_fb_pixel(ctx, fx, fy, color);
}

static uint8_t mix_channel(uint8_t a, uint8_t b, int t, int max_t) {
  if (max_t <= 0) {
    return a;
  }
  return (uint8_t)(((int)a * (max_t - t) + (int)b * t) / max_t);
}

static uint32_t mix_color(const struct fb_ctx *ctx, uint8_t ar, uint8_t ag, uint8_t ab,
                          uint8_t br, uint8_t bg, uint8_t bb, int t, int max_t) {
  return pack_color(ctx, mix_channel(ar, br, t, max_t),
                    mix_channel(ag, bg, t, max_t),
                    mix_channel(ab, bb, t, max_t));
}

static void fill_background(struct fb_ctx *ctx) {
  int x;
  int y;
  uint32_t color;
  uint32_t line;
  uint32_t dot;

  for (y = 0; y < ctx->logical_h; y++) {
    for (x = 0; x < ctx->logical_w; x++) {
      int t = (x + y) * 255 / (ctx->logical_w + ctx->logical_h);
      color = mix_color(ctx, 5, 12, 18, 13, 37, 54, t, 255);
      put_pixel(ctx, x, y, color);
    }
  }

  line = pack_color(ctx, 21, 88, 112);
  dot = pack_color(ctx, 226, 132, 42);
  for (y = 0; y < ctx->logical_h; y += 32) {
    for (x = 0; x < ctx->logical_w; x++) {
      if (((x + y) % 160) < 3) {
        put_pixel(ctx, x, y, line);
      }
    }
  }
  for (x = 0; x < ctx->logical_w; x += 26) {
    put_pixel(ctx, x, ctx->logical_h - 44, dot);
    put_pixel(ctx, x + 1, ctx->logical_h - 44, dot);
  }
}

static void fill_rect(struct fb_ctx *ctx, int x, int y, int w, int h, uint32_t color) {
  int xx;
  int yy;

  for (yy = y; yy < y + h; yy++) {
    for (xx = x; xx < x + w; xx++) {
      put_pixel(ctx, xx, yy, color);
    }
  }
}

static void draw_bar(struct fb_ctx *ctx) {
  uint32_t blue = pack_color(ctx, 34, 153, 190);
  uint32_t orange = pack_color(ctx, 232, 134, 43);
  uint32_t dark = pack_color(ctx, 9, 19, 26);
  int x = 120;
  int y = 330;
  int w = ctx->logical_w - 240;

  fill_rect(ctx, x, y, w, 5, dark);
  fill_rect(ctx, x, y, w * 7 / 10, 5, blue);
  fill_rect(ctx, x + w * 7 / 10, y, w * 3 / 10, 5, orange);
}

static const uint8_t *glyph_for(char c) {
  static const uint8_t glyph_space[7] = {0, 0, 0, 0, 0, 0, 0};
  static const uint8_t glyph_a[7] = {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
  static const uint8_t glyph_b[7] = {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e};
  static const uint8_t glyph_g[7] = {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0e};
  static const uint8_t glyph_i[7] = {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f};
  static const uint8_t glyph_l[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f};
  static const uint8_t glyph_m[7] = {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11};
  static const uint8_t glyph_n[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
  static const uint8_t glyph_o[7] = {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
  static const uint8_t glyph_p[7] = {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10};
  static const uint8_t glyph_s[7] = {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e};
  static const uint8_t glyph_t[7] = {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
  static const uint8_t glyph_u[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
  static const uint8_t glyph_0[7] = {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e};
  static const uint8_t glyph_3[7] = {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e};

  switch (c) {
  case 'A': return glyph_a;
  case 'B': return glyph_b;
  case 'G': return glyph_g;
  case 'I': return glyph_i;
  case 'L': return glyph_l;
  case 'M': return glyph_m;
  case 'N': return glyph_n;
  case 'O': return glyph_o;
  case 'P': return glyph_p;
  case 'S': return glyph_s;
  case 'T': return glyph_t;
  case 'U': return glyph_u;
  case '0': return glyph_0;
  case '3': return glyph_3;
  case ' ': return glyph_space;
  default: return glyph_space;
  }
}

static int text_width(const char *text, int scale) {
  int chars = 0;
  const char *p;

  for (p = text; *p; p++) {
    chars++;
  }
  if (chars == 0) {
    return 0;
  }
  return chars * 5 * scale + (chars - 1) * scale;
}

static void draw_text(struct fb_ctx *ctx, const char *text, int x, int y, int scale,
                      uint32_t color) {
  int cursor = x;
  const char *p;

  for (p = text; *p; p++) {
    const uint8_t *glyph = glyph_for(*p);
    int row;
    for (row = 0; row < 7; row++) {
      int col;
      for (col = 0; col < 5; col++) {
        if (glyph[row] & (1 << (4 - col))) {
          fill_rect(ctx, cursor + col * scale, y + row * scale, scale, scale, color);
        }
      }
    }
    cursor += 6 * scale;
  }
}

static void draw_splash(struct fb_ctx *ctx) {
  uint32_t white = pack_color(ctx, 235, 242, 244);
  uint32_t muted = pack_color(ctx, 122, 157, 170);
  uint32_t blue = pack_color(ctx, 36, 158, 198);
  uint32_t orange = pack_color(ctx, 232, 134, 43);
  int w;

  fill_background(ctx);

  fill_rect(ctx, 120, 136, 48, 8, orange);
  fill_rect(ctx, 120, 144, 8, 56, orange);
  fill_rect(ctx, 136, 160, 48, 8, blue);
  fill_rect(ctx, 176, 160, 8, 56, blue);

  w = text_width("PLUMOS", 11);
  draw_text(ctx, "PLUMOS", (ctx->logical_w - w) / 2, 202, 11, white);

  w = text_width("STARTING", 4);
  draw_text(ctx, "STARTING", (ctx->logical_w - w) / 2, 292, 4, muted);

  w = text_width("A30", 3);
  draw_text(ctx, "A30", (ctx->logical_w - w) / 2, 362, 3, muted);

  draw_bar(ctx);
}

static int setup_fb(struct fb_ctx *ctx, const char *fb_path) {
  size_t map_len;

  memset(ctx, 0, sizeof(*ctx));
  ctx->fd = open(fb_path, O_RDWR);
  if (ctx->fd < 0) {
    fprintf(stderr, "error: open %s: %s\n", fb_path, strerror(errno));
    return 0;
  }

  if (ioctl(ctx->fd, FBIOGET_VSCREENINFO, &ctx->var) != 0 ||
      ioctl(ctx->fd, FBIOGET_FSCREENINFO, &ctx->fix) != 0) {
    fprintf(stderr, "error: get framebuffer info: %s\n", strerror(errno));
    close(ctx->fd);
    return 0;
  }

  if (ctx->var.bits_per_pixel != 16 && ctx->var.bits_per_pixel != 32) {
    fprintf(stderr, "error: unsupported framebuffer depth: %u\n", ctx->var.bits_per_pixel);
    close(ctx->fd);
    return 0;
  }

  ctx->var.xoffset = 0;
  ctx->var.yoffset = 0;
  ctx->var.activate = FB_ACTIVATE_NOW;
  ioctl(ctx->fd, FBIOPAN_DISPLAY, &ctx->var);

  map_len = ctx->fix.smem_len;
  if (map_len == 0) {
    map_len = (size_t)ctx->fix.line_length * ctx->var.yres;
  }
  ctx->mem = mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, 0);
  if (ctx->mem == MAP_FAILED) {
    fprintf(stderr, "error: mmap framebuffer: %s\n", strerror(errno));
    close(ctx->fd);
    return 0;
  }
  ctx->mem_len = map_len;

  ctx->rotate_ccw = ctx->var.yres > ctx->var.xres;
  ctx->logical_w = ctx->rotate_ccw ? (int)ctx->var.yres : (int)ctx->var.xres;
  ctx->logical_h = ctx->rotate_ccw ? (int)ctx->var.xres : (int)ctx->var.yres;
  return 1;
}

static void cleanup_fb(struct fb_ctx *ctx) {
  if (ctx->mem && ctx->mem != MAP_FAILED) {
    munmap(ctx->mem, ctx->mem_len);
  }
  if (ctx->fd >= 0) {
    close(ctx->fd);
  }
}

static void usage(const char *argv0) {
  printf("Usage: %s [--fb PATH] [--quiet]\n", argv0);
}

int main(int argc, char **argv) {
  const char *fb_path = "/dev/fb0";
  int quiet = 0;
  struct fb_ctx ctx;
  int i;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--fb") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "error: --fb requires a path\n");
        return 2;
      }
      fb_path = argv[++i];
    } else if (strcmp(argv[i], "--quiet") == 0) {
      quiet = 1;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "error: unknown option: %s\n", argv[i]);
      usage(argv[0]);
      return 2;
    }
  }

  if (!setup_fb(&ctx, fb_path)) {
    return 1;
  }

  draw_splash(&ctx);
  msync(ctx.mem, ctx.mem_len, MS_ASYNC);
  if (!quiet) {
    printf("splash fb=%s size=%ux%u logical=%dx%d bpp=%u\n", fb_path, ctx.var.xres,
           ctx.var.yres, ctx.logical_w, ctx.logical_h, ctx.var.bits_per_pixel);
  }
  cleanup_fb(&ctx);
  return 0;
}
