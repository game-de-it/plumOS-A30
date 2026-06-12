#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "drc_core.h"
#include "replay.h"
#include "v810_cpu.h"
#include "v810_mem.h"
#include "vb_dsp.h"
#include "vb_set.h"
#include "vb_sound.h"

#define VB_SRC_W 384
#define VB_SRC_H 224
#define TARGET_FPS 50

typedef enum {
  ROTATE_CCW,
  ROTATE_CW,
  ROTATE_NONE,
} rotate_mode_t;

typedef enum {
  SCALE_FIT,
  SCALE_STRETCH,
  SCALE_INTEGER,
} scale_mode_t;

typedef enum {
  EYE_LEFT,
  EYE_RIGHT,
  EYE_BOTH,
} eye_mode_t;

typedef enum {
  COLOR_RED,
  COLOR_WHITE,
  COLOR_GREEN,
  COLOR_AMBER,
  COLOR_BLUE,
  COLOR_CUSTOM,
} color_mode_t;

typedef struct {
  color_mode_t mode;
  uint8_t r;
  uint8_t g;
  uint8_t b;
} color_config_t;

typedef struct {
  int audio_enabled;
  rotate_mode_t rotation;
  unsigned audio_latency_us;
  unsigned audio_prebuffer_chunks;
  unsigned audio_queue_chunks;
  char cpu_policy[16];
  unsigned cpu_freq;
  unsigned cpu_cores;
  int restart_required;
  int reset_requested;
} red_viper_settings_t;

typedef struct {
  int open;
  int selected;
  int top;
} menu_ctx_t;

typedef struct {
  int initialized;
  struct timespec window_start;
  unsigned emu_frames;
  unsigned render_frames;
  unsigned present_frames;
  unsigned late_frames;
  double emu_fps;
  double render_fps;
  double present_fps;
  double speed_pct;
  double late_pct;
} perf_stats_t;

typedef enum {
  MENU_RESUME,
  MENU_EYE,
  MENU_SCALE,
  MENU_ROTATION,
  MENU_WAIT_VSYNC,
  MENU_COLOR,
  MENU_FRAME_SKIP,
  MENU_FAST_FORWARD,
  MENU_FAST_FORWARD_MODE,
  MENU_VIP_OVERCLOCK,
  MENU_PERF_INFO,
  MENU_SOUND,
  MENU_AUDIO_LATENCY,
  MENU_AUDIO_PREBUFFER,
  MENU_AUDIO_QUEUE,
  MENU_CPU_POLICY,
  MENU_CPU_FREQ,
  MENU_CPU_CORES,
  MENU_RENDERER,
  MENU_RESET,
  MENU_QUIT,
  MENU_ITEM_COUNT,
} menu_item_t;

typedef struct {
  int fd;
  uint8_t *pixels;
  size_t map_size;
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
  rotate_mode_t rotation;
  scale_mode_t scale;
  eye_mode_t eye_mode;
  color_config_t color;
  int logical_w;
  int logical_h;
  int out_x;
  int out_y;
  int out_w;
  int out_h;
  int *src_x;
  int *src_y;
  uint32_t front_yoffset;
  uint32_t back_yoffset;
  int can_pan;
  int wait_vsync;
  uint32_t black;
} fb_ctx_t;

static void draw_menu(fb_ctx_t *fb, uint32_t page_yoffset,
                      const menu_ctx_t *menu,
                      const red_viper_settings_t *settings);
static void draw_perf_overlay(fb_ctx_t *fb, uint32_t page_yoffset,
                              const perf_stats_t *stats);

static volatile sig_atomic_t g_stop_requested;

static void on_signal(int sig) {
  (void)sig;
  g_stop_requested = 1;
}

static void usage(FILE *out) {
  fprintf(out,
          "Usage: red-viper-a30 [--fb PATH] [--input PATH] [--save-base NAME] "
          "[--rotation ccw|cw|none] [--scale fit|stretch|integer] "
          "[--eye left|right|both] [--color red|white|green|amber|blue|R,G,B] "
          "[--audio alsa|off] ROM\n");
}

static uint32_t pack_component(uint32_t value, struct fb_bitfield field) {
  if (field.length == 0) {
    return 0;
  }
  uint32_t max = (1u << field.length) - 1u;
  return ((value * max + 127u) / 255u) << field.offset;
}

static uint32_t pack_color(const fb_ctx_t *fb, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t v = pack_component(r, fb->var.red) |
               pack_component(g, fb->var.green) |
               pack_component(b, fb->var.blue);
  if (fb->var.transp.length > 0) {
    v |= ((1u << fb->var.transp.length) - 1u) << fb->var.transp.offset;
  }
  return v;
}

static bool logical_to_raw(const fb_ctx_t *fb, int x, int y, int *fx_out,
                           int *fy_out) {
  int fx = x;
  int fy = y;

  switch (fb->rotation) {
  case ROTATE_CCW:
    fx = y;
    fy = (int)fb->var.yres - 1 - x;
    break;
  case ROTATE_CW:
    fx = (int)fb->var.xres - 1 - y;
    fy = x;
    break;
  case ROTATE_NONE:
    break;
  }

  if (fx < 0 || fy < 0 || fx >= (int)fb->var.xres || fy >= (int)fb->var.yres) {
    return false;
  }

  *fx_out = fx;
  *fy_out = fy;
  return true;
}

static void raw_to_logical(const fb_ctx_t *fb, int fx, int fy, int *x_out,
                           int *y_out) {
  int x = fx;
  int y = fy;

  switch (fb->rotation) {
  case ROTATE_CCW:
    x = (int)fb->var.yres - 1 - fy;
    y = fx;
    break;
  case ROTATE_CW:
    x = fy;
    y = (int)fb->var.xres - 1 - fx;
    break;
  case ROTATE_NONE:
    break;
  }

  *x_out = x;
  *y_out = y;
}

static void put_raw_pixel(const fb_ctx_t *fb, int fx, int fy,
                          uint32_t page_yoffset, uint32_t color) {
  uint32_t px = (uint32_t)fx + fb->var.xoffset;
  uint32_t py = (uint32_t)fy + page_yoffset;
  if (px >= fb->var.xres_virtual || py >= fb->var.yres_virtual) {
    return;
  }

  size_t offset = (size_t)py * fb->fix.line_length +
                  (size_t)px * fb->var.bits_per_pixel / 8u;
  if (offset + fb->var.bits_per_pixel / 8u > fb->map_size) {
    return;
  }

  if (fb->var.bits_per_pixel == 32) {
    *(uint32_t *)(fb->pixels + offset) = color;
  } else if (fb->var.bits_per_pixel == 16) {
    *(uint16_t *)(fb->pixels + offset) = (uint16_t)color;
  }
}

static void put_logical_pixel_page(const fb_ctx_t *fb, int x, int y,
                                   uint32_t page_yoffset, uint32_t color) {
  int fx;
  int fy;
  if (!logical_to_raw(fb, x, y, &fx, &fy)) {
    return;
  }
  put_raw_pixel(fb, fx, fy, page_yoffset, color);
}

static int fb_configure_layout(fb_ctx_t *fb) {
  if (fb->rotation == ROTATE_NONE) {
    fb->logical_w = (int)fb->var.xres;
    fb->logical_h = (int)fb->var.yres;
  } else {
    fb->logical_w = (int)fb->var.yres;
    fb->logical_h = (int)fb->var.xres;
  }

  if (fb->logical_w <= 0 || fb->logical_h <= 0) {
    return -1;
  }

  if (fb->scale == SCALE_STRETCH) {
    fb->out_w = fb->logical_w;
    fb->out_h = fb->logical_h;
  } else if (fb->scale == SCALE_INTEGER) {
    int sx = fb->logical_w / VB_SRC_W;
    int sy = fb->logical_h / VB_SRC_H;
    int scale = sx < sy ? sx : sy;
    if (scale < 1) {
      scale = 1;
    }
    fb->out_w = VB_SRC_W * scale;
    fb->out_h = VB_SRC_H * scale;
  } else {
    fb->out_w = fb->logical_w;
    fb->out_h = VB_SRC_H * fb->out_w / VB_SRC_W;
    if (fb->out_h > fb->logical_h) {
      fb->out_h = fb->logical_h;
      fb->out_w = VB_SRC_W * fb->out_h / VB_SRC_H;
    }
  }

  if (fb->out_w < 1 || fb->out_h < 1) {
    return -1;
  }

  fb->out_x = (fb->logical_w - fb->out_w) / 2;
  fb->out_y = (fb->logical_h - fb->out_h) / 2;

  fb->src_x = calloc((size_t)fb->out_w, sizeof(*fb->src_x));
  fb->src_y = calloc((size_t)fb->out_h, sizeof(*fb->src_y));
  if (!fb->src_x || !fb->src_y) {
    return -1;
  }

  for (int x = 0; x < fb->out_w; x++) {
    fb->src_x[x] = x * VB_SRC_W / fb->out_w;
  }
  for (int y = 0; y < fb->out_h; y++) {
    fb->src_y[y] = y * VB_SRC_H / fb->out_h;
  }
  return 0;
}

static int fb_open(fb_ctx_t *fb, const char *path, rotate_mode_t rotation,
                   scale_mode_t scale, eye_mode_t eye_mode,
                   color_config_t color) {
  memset(fb, 0, sizeof(*fb));
  fb->fd = -1;
  fb->rotation = rotation;
  fb->scale = scale;
  fb->eye_mode = eye_mode;
  fb->color = color;

  fb->fd = open(path, O_RDWR);
  if (fb->fd < 0) {
    fprintf(stderr, "red-viper-a30: open %s failed: %s\n", path, strerror(errno));
    return -1;
  }
  if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->fix) < 0 ||
      ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->var) < 0) {
    fprintf(stderr, "red-viper-a30: fb ioctl failed: %s\n", strerror(errno));
    return -1;
  }
  if (fb->var.bits_per_pixel != 32 && fb->var.bits_per_pixel != 16) {
    fprintf(stderr, "red-viper-a30: unsupported fb bpp: %u\n",
            fb->var.bits_per_pixel);
    return -1;
  }

  fb->map_size = fb->fix.smem_len;
  if (fb->map_size == 0) {
    fb->map_size = (size_t)fb->fix.line_length * fb->var.yres_virtual;
  }
  fb->pixels = mmap(NULL, fb->map_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fb->fd, 0);
  if (fb->pixels == MAP_FAILED) {
    fb->pixels = NULL;
    fprintf(stderr, "red-viper-a30: fb mmap failed: %s\n", strerror(errno));
    return -1;
  }

  fb->black = pack_color(fb, 0, 0, 0);
  if (fb_configure_layout(fb) < 0) {
    fprintf(stderr, "red-viper-a30: failed to configure fb layout\n");
    return -1;
  }
  fb->front_yoffset = fb->var.yoffset;
  fb->back_yoffset = fb->front_yoffset;
  if (fb->var.yres > 0 &&
      fb->var.yres_virtual >= fb->var.yres * 2) {
    fb->can_pan = 1;
    fb->wait_vsync = 1;
    if (fb->front_yoffset >= fb->var.yres) {
      fb->front_yoffset = fb->var.yres;
      fb->back_yoffset = 0;
    } else {
      fb->front_yoffset = 0;
      fb->back_yoffset = fb->var.yres;
    }
  }
  return 0;
}

static void fb_close(fb_ctx_t *fb) {
  if (fb->pixels) {
    munmap(fb->pixels, fb->map_size);
    fb->pixels = NULL;
  }
  if (fb->fd >= 0) {
    close(fb->fd);
    fb->fd = -1;
  }
  free(fb->src_x);
  free(fb->src_y);
  fb->src_x = NULL;
  fb->src_y = NULL;
}

static void fb_clear_page(fb_ctx_t *fb, uint32_t page_yoffset) {
  uint32_t bytes_per_pixel = fb->var.bits_per_pixel / 8u;
  if (!fb->pixels || page_yoffset >= fb->var.yres_virtual ||
      bytes_per_pixel == 0) {
    return;
  }

  if (fb->black == 0) {
    for (uint32_t y = 0; y < fb->var.yres &&
                         page_yoffset + y < fb->var.yres_virtual; y++) {
      size_t offset = (size_t)(page_yoffset + y) * fb->fix.line_length +
                      (size_t)fb->var.xoffset * bytes_per_pixel;
      size_t bytes = (size_t)fb->var.xres * bytes_per_pixel;
      if (offset + bytes > fb->map_size) {
        break;
      }
      memset(fb->pixels + offset, 0, bytes);
    }
    return;
  }

  for (uint32_t y = 0; y < fb->var.yres; y++) {
    for (uint32_t x = 0; x < fb->var.xres; x++) {
      put_raw_pixel(fb, (int)x, (int)y, page_yoffset, fb->black);
    }
  }
}

static void fb_clear(fb_ctx_t *fb) {
  fb_clear_page(fb, fb->front_yoffset);
  if (fb->can_pan && fb->back_yoffset != fb->front_yoffset) {
    fb_clear_page(fb, fb->back_yoffset);
  }
}

static void fb_wait_for_vsync(fb_ctx_t *fb) {
#ifdef FBIO_WAITFORVSYNC
  if (!fb->wait_vsync) {
    return;
  }
  uint32_t arg = 0;
  if (ioctl(fb->fd, FBIO_WAITFORVSYNC, &arg) < 0) {
    fb->wait_vsync = 0;
  }
#else
  (void)fb;
#endif
}

static int fb_present_page(fb_ctx_t *fb, uint32_t page_yoffset) {
  if (!fb->can_pan || page_yoffset == fb->front_yoffset) {
    return 0;
  }

  fb_wait_for_vsync(fb);
  struct fb_var_screeninfo var = fb->var;
  var.yoffset = page_yoffset;
  if (ioctl(fb->fd, FBIOPAN_DISPLAY, &var) < 0) {
    fprintf(stderr, "red-viper-a30: FBIOPAN_DISPLAY failed, disabling pan: %s\n",
            strerror(errno));
    fb->can_pan = 0;
    fb->back_yoffset = fb->front_yoffset;
    return -1;
  }

  fb->var = var;
  fb->front_yoffset = page_yoffset;
  fb->back_yoffset = page_yoffset == 0 ? fb->var.yres : 0;
  if (fb->back_yoffset + fb->var.yres > fb->var.yres_virtual) {
    fb->back_yoffset = fb->front_yoffset;
    fb->can_pan = 0;
  }
  return 0;
}

static void build_palette(const fb_ctx_t *fb, uint32_t palette[4]) {
  int brt[4] = {
      0,
      vb_state->tVIPREG.BRTA,
      vb_state->tVIPREG.BRTB,
      vb_state->tVIPREG.BRTA + vb_state->tVIPREG.BRTB + vb_state->tVIPREG.BRTC,
  };
  for (int i = 0; i < 4; i++) {
    int level = brt[i] * 2;
    if (level > 255) {
      level = 255;
    }
    palette[i] = pack_color(fb, (uint8_t)((fb->color.r * level + 127) / 255),
                            (uint8_t)((fb->color.g * level + 127) / 255),
                            (uint8_t)((fb->color.b * level + 127) / 255));
  }
}

static int vb_framebuffer_pixel(const uint16_t *vb_fb, int sx, int sy) {
  int word_y = sy >> 3;
  int shift = (sy & 7) * 2;
  uint16_t word = vb_fb[sx * 32 + word_y];
  return (word >> shift) & 3;
}

static uint32_t vb_framebuffer_color(const fb_ctx_t *fb, const uint16_t *left_fb,
                                     const uint16_t *right_fb, int sx, int sy,
                                     const uint32_t palette[4]) {
  int colour = 0;
  switch (fb->eye_mode) {
  case EYE_RIGHT:
    colour = vb_framebuffer_pixel(right_fb, sx, sy);
    break;
  case EYE_BOTH:
    colour = vb_framebuffer_pixel(left_fb, sx, sy);
    {
      int right_colour = vb_framebuffer_pixel(right_fb, sx, sy);
      if (right_colour > colour) {
        colour = right_colour;
      }
    }
    break;
  case EYE_LEFT:
  default:
    colour = vb_framebuffer_pixel(left_fb, sx, sy);
    break;
  }
  return palette[colour];
}

static void fb_blit_vb(fb_ctx_t *fb, int fb_index, const menu_ctx_t *menu,
                       const red_viper_settings_t *settings,
                       const perf_stats_t *stats) {
  const uint16_t *left_fb =
      (const uint16_t *)(vb_players[0].V810_DISPLAY_RAM.off +
                         0x8000 * (fb_index & 1));
  const uint16_t *right_fb =
      (const uint16_t *)(vb_players[0].V810_DISPLAY_RAM.off + 0x10000 +
                         0x8000 * (fb_index & 1));
  uint32_t palette[4];
  uint32_t target_yoffset = fb->can_pan ? fb->back_yoffset : fb->front_yoffset;
  build_palette(fb, palette);

retry:
  for (uint32_t raw_y = 0; raw_y < fb->var.yres; raw_y++) {
    uint32_t py = target_yoffset + raw_y;
    size_t offset = (size_t)py * fb->fix.line_length +
                    (size_t)fb->var.xoffset * fb->var.bits_per_pixel / 8u;
    size_t row_bytes = (size_t)fb->var.xres * fb->var.bits_per_pixel / 8u;
    if (py >= fb->var.yres_virtual || offset + row_bytes > fb->map_size) {
      break;
    }

    if (fb->var.bits_per_pixel == 32) {
      uint32_t *dst = (uint32_t *)(fb->pixels + offset);
      for (uint32_t raw_x = 0; raw_x < fb->var.xres; raw_x++) {
        int logical_x;
        int logical_y;
        raw_to_logical(fb, (int)raw_x, (int)raw_y, &logical_x, &logical_y);
        int out_x = logical_x - fb->out_x;
        int out_y = logical_y - fb->out_y;
        uint32_t color = fb->black;
        if ((unsigned)out_x < (unsigned)fb->out_w &&
            (unsigned)out_y < (unsigned)fb->out_h) {
          color = vb_framebuffer_color(fb, left_fb, right_fb, fb->src_x[out_x],
                                       fb->src_y[out_y], palette);
        }
        dst[raw_x] = color;
      }
    } else {
      uint16_t *dst = (uint16_t *)(fb->pixels + offset);
      for (uint32_t raw_x = 0; raw_x < fb->var.xres; raw_x++) {
        int logical_x;
        int logical_y;
        raw_to_logical(fb, (int)raw_x, (int)raw_y, &logical_x, &logical_y);
        int out_x = logical_x - fb->out_x;
        int out_y = logical_y - fb->out_y;
        uint32_t color = fb->black;
        if ((unsigned)out_x < (unsigned)fb->out_w &&
            (unsigned)out_y < (unsigned)fb->out_h) {
          color = vb_framebuffer_color(fb, left_fb, right_fb, fb->src_x[out_x],
                                       fb->src_y[out_y], palette);
        }
        dst[raw_x] = (uint16_t)color;
      }
    }
  }
  if (menu && menu->open) {
    draw_menu(fb, target_yoffset, menu, settings);
  } else if (tVBOpt.PERF_INFO && stats) {
    draw_perf_overlay(fb, target_yoffset, stats);
  }
  if (fb->can_pan && fb_present_page(fb, target_yoffset) < 0) {
    target_yoffset = fb->front_yoffset;
    goto retry;
  }
}

static void render_vip_frame(int draw_fb) {
  if (tDSPCACHE.CharCacheInvalid) {
    update_texture_cache_soft();
  }
  video_soft_render(draw_fb & 1);
  tDSPCACHE.CharCacheInvalid = false;
  tDSPCACHE.CharCacheForceInvalid = false;
  tDSPCACHE.ColumnTableInvalid = false;
  tDSPCACHE.BrtPALMod = false;
  memset(tDSPCACHE.CharacterCache, 0, sizeof(tDSPCACHE.CharacterCache));
}

static const char *eye_mode_name(eye_mode_t mode) {
  switch (mode) {
  case EYE_LEFT:
    return "left";
  case EYE_RIGHT:
    return "right";
  case EYE_BOTH:
  default:
    return "both";
  }
}

static const char *scale_mode_name(scale_mode_t mode) {
  switch (mode) {
  case SCALE_STRETCH:
    return "stretch";
  case SCALE_INTEGER:
    return "integer";
  case SCALE_FIT:
  default:
    return "fit";
  }
}

static const char *rotation_mode_name(rotate_mode_t mode) {
  switch (mode) {
  case ROTATE_CW:
    return "cw";
  case ROTATE_NONE:
    return "none";
  case ROTATE_CCW:
  default:
    return "ccw";
  }
}

static const char *color_mode_name(color_mode_t mode) {
  switch (mode) {
  case COLOR_WHITE:
    return "white";
  case COLOR_GREEN:
    return "green";
  case COLOR_AMBER:
    return "amber";
  case COLOR_BLUE:
    return "blue";
  case COLOR_CUSTOM:
    return "custom";
  case COLOR_RED:
  default:
    return "red";
  }
}

static int color_to_tint(const color_config_t *color) {
  return color->r | (color->g << 8) | (color->b << 16);
}

static int starts_with_key(const char *line, const char *key) {
  size_t key_len = strlen(key);
  return strncmp(line, key, key_len) == 0 && line[key_len] == '=';
}

static void ensure_red_viper_config_dir(const char *root) {
  char path[PATH_MAX];

  snprintf(path, sizeof(path), "%s/config", root);
  mkdir(path, 0777);
  snprintf(path, sizeof(path), "%s/config/standalone", root);
  mkdir(path, 0777);
}

static void persist_setting(const char *key, const char *value) {
  const char *root = getenv("PLUMOS_ROOT");
  char path[PATH_MAX];
  char tmp_path[PATH_MAX];
  FILE *in = NULL;
  FILE *out = NULL;
  char line[512];
  int replaced = 0;

  if (!root || !*root) {
    root = "/mnt/SDCARD/plumos";
  }
  ensure_red_viper_config_dir(root);
  snprintf(path, sizeof(path), "%s/config/standalone/red_viper.env", root);
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

  in = fopen(path, "r");
  out = fopen(tmp_path, "w");
  if (!out) {
    if (in) {
      fclose(in);
    }
    return;
  }

  if (in) {
    while (fgets(line, sizeof(line), in)) {
      if (starts_with_key(line, key)) {
        fprintf(out, "%s=%s\n", key, value);
        replaced = 1;
      } else {
        fputs(line, out);
      }
    }
    fclose(in);
  }

  if (!replaced) {
    fprintf(out, "%s=%s\n", key, value);
  }
  fclose(out);
  rename(tmp_path, path);
}

static void persist_uint_setting(const char *key, unsigned value) {
  char text[32];
  snprintf(text, sizeof(text), "%u", value);
  persist_setting(key, text);
}

static void set_color_preset(color_config_t *color, color_mode_t mode) {
  color->mode = mode;
  switch (mode) {
  case COLOR_WHITE:
    color->r = 255;
    color->g = 255;
    color->b = 255;
    break;
  case COLOR_GREEN:
    color->r = 0;
    color->g = 255;
    color->b = 64;
    break;
  case COLOR_AMBER:
    color->r = 255;
    color->g = 176;
    color->b = 0;
    break;
  case COLOR_BLUE:
    color->r = 64;
    color->g = 160;
    color->b = 255;
    break;
  case COLOR_RED:
  default:
    color->mode = COLOR_RED;
    color->r = 255;
    color->g = 0;
    color->b = 0;
    break;
  }
}

static void color_setting_value(const color_config_t *color, char *out,
                                size_t out_size) {
  if (color->mode == COLOR_CUSTOM) {
    snprintf(out, out_size, "%u,%u,%u", color->r, color->g, color->b);
  } else {
    snprintf(out, out_size, "%s", color_mode_name(color->mode));
  }
}

static void cycle_eye(fb_ctx_t *fb, int delta) {
  int value = (int)fb->eye_mode + delta;
  while (value < 0) {
    value += 3;
  }
  fb->eye_mode = (eye_mode_t)(value % 3);
  persist_setting("PLUMOS_A30_RED_VIPER_EYE", eye_mode_name(fb->eye_mode));
}

static void cycle_color(fb_ctx_t *fb, int delta) {
  char value_text[32];
  int value = fb->color.mode == COLOR_CUSTOM ? 0 : (int)fb->color.mode;
  value += delta;
  while (value < 0) {
    value += 5;
  }
  set_color_preset(&fb->color, (color_mode_t)(value % 5));
  tVBOpt.TINT = color_to_tint(&fb->color);
  color_setting_value(&fb->color, value_text, sizeof(value_text));
  persist_setting("PLUMOS_A30_RED_VIPER_COLOR", value_text);
}

static int reconfigure_layout(fb_ctx_t *fb, rotate_mode_t rotation,
                              scale_mode_t scale) {
  rotate_mode_t old_rotation = fb->rotation;
  scale_mode_t old_scale = fb->scale;
  int *old_src_x = fb->src_x;
  int *old_src_y = fb->src_y;

  fb->rotation = rotation;
  fb->scale = scale;
  fb->src_x = NULL;
  fb->src_y = NULL;
  if (fb_configure_layout(fb) == 0) {
    free(old_src_x);
    free(old_src_y);
    fb_clear(fb);
    return 0;
  }

  free(fb->src_x);
  free(fb->src_y);
  fb->src_x = old_src_x;
  fb->src_y = old_src_y;
  fb->rotation = old_rotation;
  fb->scale = old_scale;
  return -1;
}

static void cycle_rotation(fb_ctx_t *fb, red_viper_settings_t *settings,
                           int delta) {
  int value = (int)fb->rotation + delta;
  while (value < 0) {
    value += 3;
  }
  rotate_mode_t rotation = (rotate_mode_t)(value % 3);
  if (reconfigure_layout(fb, rotation, fb->scale) == 0) {
    settings->rotation = fb->rotation;
    persist_setting("PLUMOS_A30_RED_VIPER_ROTATION",
                    rotation_mode_name(fb->rotation));
  }
}

static void cycle_scale(fb_ctx_t *fb, int delta) {
  int value = (int)fb->scale + delta;
  while (value < 0) {
    value += 3;
  }
  if (reconfigure_layout(fb, fb->rotation, (scale_mode_t)(value % 3)) == 0) {
    persist_setting("PLUMOS_A30_RED_VIPER_SCALE", scale_mode_name(fb->scale));
  }
}

static void cycle_wait_vsync(fb_ctx_t *fb) {
  fb->wait_vsync = !fb->wait_vsync;
  persist_uint_setting("PLUMOS_A30_RED_VIPER_WAIT_VSYNC",
                       fb->wait_vsync ? 1u : 0u);
}

static unsigned cycle_choice_unsigned(unsigned current, const unsigned *values,
                                      size_t count, int delta) {
  size_t index = 0;

  for (size_t i = 0; i < count; i++) {
    if (values[i] == current) {
      index = i;
      break;
    }
  }
  if (delta < 0) {
    index = (index + count - 1) % count;
  } else {
    index = (index + 1) % count;
  }
  return values[index];
}

static void mark_restart_setting(red_viper_settings_t *settings) {
  settings->restart_required = 1;
}

static void cycle_frame_skip(int delta) {
  int value = tVBOpt.FRMSKIP + delta;
  while (value < 0) {
    value += 4;
  }
  tVBOpt.FRMSKIP = value % 4;
  persist_uint_setting("PLUMOS_A30_RED_VIPER_FRAME_SKIP",
                       (unsigned)tVBOpt.FRMSKIP);
}

static void cycle_fast_forward(void) {
  tVBOpt.FASTFORWARD = !tVBOpt.FASTFORWARD;
  persist_uint_setting("PLUMOS_A30_RED_VIPER_FAST_FORWARD",
                       (unsigned)tVBOpt.FASTFORWARD);
}

static void cycle_fast_forward_mode(void) {
  tVBOpt.FF_TOGGLE = !tVBOpt.FF_TOGGLE;
  persist_uint_setting("PLUMOS_A30_RED_VIPER_FF_TOGGLE",
                       (unsigned)tVBOpt.FF_TOGGLE);
}

static void cycle_vip_overclock(void) {
  tVBOpt.VIP_OVERCLOCK = !tVBOpt.VIP_OVERCLOCK;
  persist_uint_setting("PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK",
                       tVBOpt.VIP_OVERCLOCK ? 1u : 0u);
}

static void cycle_perf_info(void) {
  tVBOpt.PERF_INFO = !tVBOpt.PERF_INFO;
  persist_uint_setting("PLUMOS_A30_RED_VIPER_PERF_INFO",
                       tVBOpt.PERF_INFO ? 1u : 0u);
}

static void cycle_sound(red_viper_settings_t *settings) {
  settings->audio_enabled = !settings->audio_enabled;
  tVBOpt.SOUND = settings->audio_enabled ? 1 : 0;
  if (settings->audio_enabled) {
    sound_resume();
    persist_setting("PLUMOS_A30_RED_VIPER_AUDIO", "alsa");
  } else {
    sound_pause();
    persist_setting("PLUMOS_A30_RED_VIPER_AUDIO", "off");
  }
}

static void cycle_audio_latency(red_viper_settings_t *settings, int delta) {
  static const unsigned values[] = {120000, 160000, 240000, 320000, 500000};
  settings->audio_latency_us =
      cycle_choice_unsigned(settings->audio_latency_us, values,
                            sizeof(values) / sizeof(values[0]), delta);
  persist_uint_setting("PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US",
                       settings->audio_latency_us);
  mark_restart_setting(settings);
}

static void cycle_audio_prebuffer(red_viper_settings_t *settings, int delta) {
  static const unsigned values[] = {0, 2, 4, 6, 8};
  settings->audio_prebuffer_chunks =
      cycle_choice_unsigned(settings->audio_prebuffer_chunks, values,
                            sizeof(values) / sizeof(values[0]), delta);
  persist_uint_setting("PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS",
                       settings->audio_prebuffer_chunks);
  mark_restart_setting(settings);
}

static void cycle_audio_queue(red_viper_settings_t *settings, int delta) {
  static const unsigned values[] = {2, 4, 8, 12, 16, 24, 32};
  settings->audio_queue_chunks =
      cycle_choice_unsigned(settings->audio_queue_chunks, values,
                            sizeof(values) / sizeof(values[0]), delta);
  persist_uint_setting("PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS",
                       settings->audio_queue_chunks);
  mark_restart_setting(settings);
}

static void cycle_cpu_policy(red_viper_settings_t *settings) {
  if (strcmp(settings->cpu_policy, "performance") == 0) {
    snprintf(settings->cpu_policy, sizeof(settings->cpu_policy), "fixed");
  } else {
    snprintf(settings->cpu_policy, sizeof(settings->cpu_policy), "performance");
  }
  persist_setting("PLUMOS_A30_RED_VIPER_CPU_POLICY", settings->cpu_policy);
  mark_restart_setting(settings);
}

static void cycle_cpu_freq(red_viper_settings_t *settings, int delta) {
  static const unsigned values[] = {648000, 816000, 1008000, 1200000, 1344000};
  settings->cpu_freq =
      cycle_choice_unsigned(settings->cpu_freq, values,
                            sizeof(values) / sizeof(values[0]), delta);
  persist_uint_setting("PLUMOS_A30_RED_VIPER_CPU_FREQ", settings->cpu_freq);
  mark_restart_setting(settings);
}

static void cycle_cpu_cores(red_viper_settings_t *settings, int delta) {
  static const unsigned values[] = {2, 4};
  settings->cpu_cores =
      cycle_choice_unsigned(settings->cpu_cores, values,
                            sizeof(values) / sizeof(values[0]), delta);
  persist_uint_setting("PLUMOS_A30_RED_VIPER_CPU_CORES", settings->cpu_cores);
  mark_restart_setting(settings);
}

static double timespec_elapsed_seconds(const struct timespec *start,
                                       const struct timespec *end) {
  return (double)(end->tv_sec - start->tv_sec) +
         (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static void perf_stats_reset(perf_stats_t *stats) {
  memset(stats, 0, sizeof(*stats));
  clock_gettime(CLOCK_MONOTONIC, &stats->window_start);
  stats->initialized = 1;
}

static void perf_stats_update(perf_stats_t *stats, int rendered, int presented,
                              int late) {
  struct timespec now;
  double elapsed;

  if (!stats->initialized) {
    perf_stats_reset(stats);
  }

  stats->emu_frames++;
  if (rendered) {
    stats->render_frames++;
  }
  if (presented) {
    stats->present_frames++;
  }
  if (late) {
    stats->late_frames++;
  }

  clock_gettime(CLOCK_MONOTONIC, &now);
  elapsed = timespec_elapsed_seconds(&stats->window_start, &now);
  if (elapsed < 1.0) {
    return;
  }

  stats->emu_fps = (double)stats->emu_frames / elapsed;
  stats->render_fps = (double)stats->render_frames / elapsed;
  stats->present_fps = (double)stats->present_frames / elapsed;
  stats->speed_pct = stats->emu_fps * 100.0 / (double)TARGET_FPS;
  stats->late_pct = stats->emu_frames
                        ? (double)stats->late_frames * 100.0 /
                              (double)stats->emu_frames
                        : 0.0;
  stats->emu_frames = 0;
  stats->render_frames = 0;
  stats->present_frames = 0;
  stats->late_frames = 0;
  stats->window_start = now;
}

static uint8_t font5x7_row(char c, int row) {
  static const uint8_t rows[][8] = {
      {'A', 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
      {'B', 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e},
      {'C', 0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f},
      {'D', 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e},
      {'E', 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f},
      {'F', 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10},
      {'G', 0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f},
      {'H', 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
      {'I', 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f},
      {'J', 0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e},
      {'K', 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
      {'L', 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f},
      {'M', 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11},
      {'N', 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
      {'O', 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
      {'P', 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10},
      {'Q', 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d},
      {'R', 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11},
      {'S', 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e},
      {'T', 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
      {'U', 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
      {'V', 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04},
      {'W', 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a},
      {'X', 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11},
      {'Y', 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04},
      {'Z', 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f},
      {'0', 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e},
      {'1', 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
      {'2', 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
      {'3', 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
      {'4', 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
      {'5', 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e},
      {'6', 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e},
      {'7', 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
      {'8', 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
      {'9', 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e},
      {':', 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00},
      {'-', 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00},
      {'<', 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02},
      {'>', 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08},
      {'/', 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10},
  };
  if (c >= 'a' && c <= 'z') {
    c = (char)(c - 'a' + 'A');
  }
  if (c == ' ') {
    return 0;
  }
  for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
    if (rows[i][0] == (uint8_t)c) {
      return rows[i][row + 1];
    }
  }
  return row == 6 ? 0x1f : 0x11;
}

static void draw_rect(fb_ctx_t *fb, uint32_t page_yoffset, int x, int y, int w,
                      int h, uint32_t color) {
  for (int yy = 0; yy < h; yy++) {
    for (int xx = 0; xx < w; xx++) {
      put_logical_pixel_page(fb, x + xx, y + yy, page_yoffset, color);
    }
  }
}

static void draw_text(fb_ctx_t *fb, uint32_t page_yoffset, int x, int y,
                      const char *text, int scale, uint32_t color) {
  for (const char *p = text; *p; p++, x += 6 * scale) {
    for (int row = 0; row < 7; row++) {
      uint8_t bits = font5x7_row(*p, row);
      for (int col = 0; col < 5; col++) {
        if (!(bits & (1u << (4 - col)))) {
          continue;
        }
        draw_rect(fb, page_yoffset, x + col * scale, y + row * scale, scale,
                  scale, color);
      }
    }
  }
}

static void draw_perf_overlay(fb_ctx_t *fb, uint32_t page_yoffset,
                              const perf_stats_t *stats) {
  uint32_t panel = pack_color(fb, 0, 0, 0);
  uint32_t border = pack_color(fb, 120, 120, 120);
  uint32_t text = pack_color(fb, 255, 255, 255);
  uint32_t warn = pack_color(fb, 255, 176, 0);
  uint32_t alert = pack_color(fb, 255, 64, 64);
  uint32_t speed_color = text;
  char line[80];
  int x = 8;
  int y = 8;
  int w = 292;
  int h = 86;

  if (!stats) {
    return;
  }
  if (stats->speed_pct < 95.0) {
    speed_color = warn;
  } else if (stats->speed_pct > 105.0) {
    speed_color = alert;
  }

  draw_rect(fb, page_yoffset, x, y, w, h, panel);
  draw_rect(fb, page_yoffset, x, y, w, 2, border);
  draw_rect(fb, page_yoffset, x, y + h - 2, w, 2, border);
  draw_rect(fb, page_yoffset, x, y, 2, h, border);
  draw_rect(fb, page_yoffset, x + w - 2, y, 2, h, border);

  snprintf(line, sizeof(line), "EMU %4.1f  SPD %3.0f%%", stats->emu_fps,
           stats->speed_pct);
  draw_text(fb, page_yoffset, x + 10, y + 10, line, 2, speed_color);
  snprintf(line, sizeof(line), "PRES %4.1f  REND %4.1f", stats->present_fps,
           stats->render_fps);
  draw_text(fb, page_yoffset, x + 10, y + 34, line, 2, text);
  snprintf(line, sizeof(line), "SKIP %d  LATE %3.0f%%", tVBOpt.FRMSKIP,
           stats->late_pct);
  draw_text(fb, page_yoffset, x + 10, y + 58, line, 2,
            stats->late_pct > 0.0 ? warn : text);
}

static const char *on_off(int enabled) {
  return enabled ? "on" : "off";
}

static const char *fast_forward_mode_name(void) {
  return tVBOpt.FF_TOGGLE ? "toggle" : "hold";
}

static void menu_item_text(const fb_ctx_t *fb,
                           const red_viper_settings_t *settings, int index,
                           char *out, size_t out_size) {
  char color_value[32];

  switch (index) {
  case MENU_RESUME:
    snprintf(out, out_size, "RESUME");
    break;
  case MENU_EYE:
    snprintf(out, out_size, "EYE        < %s >", eye_mode_name(fb->eye_mode));
    break;
  case MENU_SCALE:
    snprintf(out, out_size, "SCALE      < %s >", scale_mode_name(fb->scale));
    break;
  case MENU_ROTATION:
    snprintf(out, out_size, "ROTATION   < %s >",
             rotation_mode_name(settings->rotation));
    break;
  case MENU_WAIT_VSYNC:
    snprintf(out, out_size, "WAIT VSYNC < %s >", on_off(fb->wait_vsync));
    break;
  case MENU_COLOR:
    color_setting_value(&fb->color, color_value, sizeof(color_value));
    snprintf(out, out_size, "COLOR      < %s >", color_value);
    break;
  case MENU_FRAME_SKIP:
    snprintf(out, out_size, "FRAME SKIP < %d >", tVBOpt.FRMSKIP);
    break;
  case MENU_FAST_FORWARD:
    snprintf(out, out_size, "FAST FWD   < %s >", on_off(tVBOpt.FASTFORWARD));
    break;
  case MENU_FAST_FORWARD_MODE:
    snprintf(out, out_size, "FF MODE    < %s >", fast_forward_mode_name());
    break;
  case MENU_VIP_OVERCLOCK:
    snprintf(out, out_size, "VIP OC     < %s >",
             on_off(tVBOpt.VIP_OVERCLOCK));
    break;
  case MENU_PERF_INFO:
    snprintf(out, out_size, "PERF INFO  < %s >", on_off(tVBOpt.PERF_INFO));
    break;
  case MENU_SOUND:
    snprintf(out, out_size, "SOUND      < %s >",
             on_off(settings->audio_enabled));
    break;
  case MENU_AUDIO_LATENCY:
    snprintf(out, out_size, "AUDIO LAT  < %ums >",
             settings->audio_latency_us / 1000);
    break;
  case MENU_AUDIO_PREBUFFER:
    snprintf(out, out_size, "PREBUFFER  < %u >",
             settings->audio_prebuffer_chunks);
    break;
  case MENU_AUDIO_QUEUE:
    snprintf(out, out_size, "QUEUE      < %u >",
             settings->audio_queue_chunks);
    break;
  case MENU_CPU_POLICY:
    snprintf(out, out_size, "CPU POLICY < %s >", settings->cpu_policy);
    break;
  case MENU_CPU_FREQ:
    snprintf(out, out_size, "CPU FREQ   < %uMHZ >",
             settings->cpu_freq / 1000);
    break;
  case MENU_CPU_CORES:
    snprintf(out, out_size, "CPU CORES  < %u >", settings->cpu_cores);
    break;
  case MENU_RENDERER:
    snprintf(out, out_size, "RENDERER   CPU ONLY");
    break;
  case MENU_RESET:
    snprintf(out, out_size, "RESET GAME");
    break;
  case MENU_QUIT:
  default:
    snprintf(out, out_size, "QUIT");
    break;
  }
}

static int menu_visible_rows(const fb_ctx_t *fb, int panel_h) {
  int rows = (panel_h - 120) / 34;
  if (rows < 4) {
    rows = 4;
  }
  if (rows > MENU_ITEM_COUNT) {
    rows = MENU_ITEM_COUNT;
  }
  (void)fb;
  return rows;
}

static void draw_menu(fb_ctx_t *fb, uint32_t page_yoffset,
                      const menu_ctx_t *menu,
                      const red_viper_settings_t *settings) {
  int panel_x = fb->logical_w / 10;
  int panel_y = fb->logical_h / 8;
  int panel_w = fb->logical_w - panel_x * 2;
  int panel_h = fb->logical_h - panel_y * 2;
  int rows = menu_visible_rows(fb, panel_h);
  uint32_t panel = pack_color(fb, 0, 0, 0);
  uint32_t border = pack_color(fb, 180, 180, 180);
  uint32_t active = pack_color(fb, 64, 96, 160);
  uint32_t text = pack_color(fb, 255, 255, 255);
  uint32_t muted = pack_color(fb, 150, 150, 150);
  uint32_t title = pack_color(fb, 255, 64, 64);
  char line[80];

  draw_rect(fb, page_yoffset, panel_x, panel_y, panel_w, panel_h, panel);
  draw_rect(fb, page_yoffset, panel_x, panel_y, panel_w, 3, border);
  draw_rect(fb, page_yoffset, panel_x, panel_y + panel_h - 3, panel_w, 3,
            border);
  draw_rect(fb, page_yoffset, panel_x, panel_y, 3, panel_h, border);
  draw_rect(fb, page_yoffset, panel_x + panel_w - 3, panel_y, 3, panel_h,
            border);

  draw_text(fb, page_yoffset, panel_x + 24, panel_y + 20, "RED VIPER", 3,
            title);
  for (int row = 0; row < rows; row++) {
    int i = menu->top + row;
    int y = panel_y + 78 + row * 34;
    menu_item_text(fb, settings, i, line, sizeof(line));
    if (i == menu->selected) {
      draw_rect(fb, page_yoffset, panel_x + 18, y - 8, panel_w - 36, 34,
                active);
    }
    draw_text(fb, page_yoffset, panel_x + 32, y, line, 2, text);
  }
  if (menu->top > 0) {
    draw_text(fb, page_yoffset, panel_x + panel_w - 72, panel_y + 78, "UP", 2,
              muted);
  }
  if (menu->top + rows < MENU_ITEM_COUNT) {
    draw_text(fb, page_yoffset, panel_x + panel_w - 96,
              panel_y + 78 + (rows - 1) * 34, "DOWN", 2, muted);
  }
  if (settings->restart_required) {
    draw_text(fb, page_yoffset, panel_x + 32, panel_y + panel_h - 66,
              "RESTART NEEDED", 2, title);
  }
  draw_text(fb, page_yoffset, panel_x + 32, panel_y + panel_h - 42,
            "A SELECT  B/FN BACK", 2, border);
}

static int open_input(const char *path) {
  if (!path || strcmp(path, "none") == 0) {
    return -1;
  }
  int fd = open(path, O_RDONLY | O_NONBLOCK);
  if (fd >= 0) {
    return fd;
  }
  fprintf(stderr, "red-viper-a30: input open %s failed: %s\n", path,
          strerror(errno));
  return -1;
}

static int key_to_vb_flag(unsigned code) {
  switch (code) {
  case KEY_UP:
  case BTN_DPAD_UP:
    return VB_LPAD_U;
  case KEY_DOWN:
  case BTN_DPAD_DOWN:
    return VB_LPAD_D;
  case KEY_LEFT:
  case BTN_DPAD_LEFT:
    return VB_LPAD_L;
  case KEY_RIGHT:
  case BTN_DPAD_RIGHT:
    return VB_LPAD_R;
  case KEY_SPACE:
  case KEY_Z:
  case BTN_SOUTH:
    return VB_KEY_A;
  case KEY_LEFTCTRL:
  case KEY_X:
  case BTN_EAST:
    return VB_KEY_B;
  case KEY_ENTER:
  case BTN_START:
    return VB_KEY_START;
  case KEY_RIGHTCTRL:
  case BTN_SELECT:
    return VB_KEY_SELECT;
  case KEY_TAB:
  case BTN_TL:
    return VB_KEY_L;
  case KEY_BACKSPACE:
  case BTN_TR:
    return VB_KEY_R;
  case KEY_LEFTSHIFT:
  case KEY_I:
  case BTN_NORTH:
    return VB_RPAD_U;
  case KEY_LEFTALT:
  case KEY_J:
  case BTN_WEST:
    return VB_RPAD_L;
  case KEY_E:
  case KEY_K:
  case BTN_TL2:
    return VB_RPAD_D;
  case KEY_T:
  case KEY_L:
  case BTN_TR2:
    return VB_RPAD_R;
  default:
    return 0;
  }
}

static void apply_input_state(uint16_t state) {
  vb_players[0].tHReg.SLB = state & 0xff;
  vb_players[0].tHReg.SHB = state >> 8;
}

static int key_is_up(unsigned code) {
  return code == KEY_UP || code == BTN_DPAD_UP;
}

static int key_is_down(unsigned code) {
  return code == KEY_DOWN || code == BTN_DPAD_DOWN;
}

static int key_is_left(unsigned code) {
  return code == KEY_LEFT || code == BTN_DPAD_LEFT;
}

static int key_is_right(unsigned code) {
  return code == KEY_RIGHT || code == BTN_DPAD_RIGHT;
}

static int key_is_accept(unsigned code) {
  return code == KEY_SPACE || code == KEY_Z || code == BTN_SOUTH;
}

static int key_is_cancel(unsigned code) {
  return code == KEY_LEFTCTRL || code == KEY_X || code == BTN_EAST;
}

static int key_is_function(unsigned code) {
  return code == KEY_ESC || code == KEY_MENU || code == BTN_MODE;
}

static void menu_update_top(menu_ctx_t *menu, const fb_ctx_t *fb) {
  int panel_y = fb->logical_h / 8;
  int panel_h = fb->logical_h - panel_y * 2;
  int rows = menu_visible_rows(fb, panel_h);

  if (menu->selected < 0) {
    menu->selected = 0;
  } else if (menu->selected >= MENU_ITEM_COUNT) {
    menu->selected = MENU_ITEM_COUNT - 1;
  }
  if (menu->top > menu->selected) {
    menu->top = menu->selected;
  }
  if (menu->selected >= menu->top + rows) {
    menu->top = menu->selected - rows + 1;
  }
  if (menu->top < 0) {
    menu->top = 0;
  }
}

static void menu_move(menu_ctx_t *menu, const fb_ctx_t *fb, int delta) {
  menu->selected += delta;
  while (menu->selected < 0) {
    menu->selected += MENU_ITEM_COUNT;
  }
  menu->selected %= MENU_ITEM_COUNT;
  menu_update_top(menu, fb);
}

static void menu_adjust(menu_ctx_t *menu, fb_ctx_t *fb,
                        red_viper_settings_t *settings, int delta) {
  switch (menu->selected) {
  case MENU_EYE:
    cycle_eye(fb, delta);
    break;
  case MENU_SCALE:
    cycle_scale(fb, delta);
    break;
  case MENU_ROTATION:
    cycle_rotation(fb, settings, delta);
    break;
  case MENU_WAIT_VSYNC:
    cycle_wait_vsync(fb);
    break;
  case MENU_COLOR:
    cycle_color(fb, delta);
    break;
  case MENU_FRAME_SKIP:
    cycle_frame_skip(delta);
    break;
  case MENU_FAST_FORWARD:
    cycle_fast_forward();
    break;
  case MENU_FAST_FORWARD_MODE:
    cycle_fast_forward_mode();
    break;
  case MENU_VIP_OVERCLOCK:
    cycle_vip_overclock();
    break;
  case MENU_PERF_INFO:
    cycle_perf_info();
    break;
  case MENU_SOUND:
    cycle_sound(settings);
    break;
  case MENU_AUDIO_LATENCY:
    cycle_audio_latency(settings, delta);
    break;
  case MENU_AUDIO_PREBUFFER:
    cycle_audio_prebuffer(settings, delta);
    break;
  case MENU_AUDIO_QUEUE:
    cycle_audio_queue(settings, delta);
    break;
  case MENU_CPU_POLICY:
    cycle_cpu_policy(settings);
    break;
  case MENU_CPU_FREQ:
    cycle_cpu_freq(settings, delta);
    break;
  case MENU_CPU_CORES:
    cycle_cpu_cores(settings, delta);
    break;
  default:
    break;
  }
}

static void menu_accept(menu_ctx_t *menu, fb_ctx_t *fb,
                        red_viper_settings_t *settings) {
  switch (menu->selected) {
  case MENU_RESUME:
    menu->open = 0;
    break;
  case MENU_RESET:
    settings->reset_requested = 1;
    menu->open = 0;
    break;
  case MENU_QUIT:
    g_stop_requested = 1;
    break;
  case MENU_RENDERER:
    break;
  default:
    menu_adjust(menu, fb, settings, 1);
    break;
  }
}

static void poll_input(int input_fd, uint16_t *state, menu_ctx_t *menu,
                       fb_ctx_t *fb, red_viper_settings_t *settings) {
  if (input_fd < 0) {
    return;
  }

  for (;;) {
    struct input_event ev;
    ssize_t n = read(input_fd, &ev, sizeof(ev));
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        break;
      }
      g_stop_requested = 1;
      break;
    }
    if (n != sizeof(ev)) {
      break;
    }
    if (ev.type != EV_KEY) {
      continue;
    }
    if (ev.value != 0 && key_is_function(ev.code)) {
      menu->open = !menu->open;
      *state = 0;
      apply_input_state(*state);
      menu_update_top(menu, fb);
      continue;
    }
    if (menu->open) {
      if (ev.value == 0) {
        continue;
      }
      if (key_is_up(ev.code)) {
        menu_move(menu, fb, -1);
      } else if (key_is_down(ev.code)) {
        menu_move(menu, fb, 1);
      } else if (key_is_left(ev.code)) {
        menu_adjust(menu, fb, settings, -1);
      } else if (key_is_right(ev.code)) {
        menu_adjust(menu, fb, settings, 1);
      } else if (key_is_accept(ev.code)) {
        menu_accept(menu, fb, settings);
      } else if (key_is_cancel(ev.code)) {
        menu->open = 0;
      }
      *state = 0;
      continue;
    }
    int flag = key_to_vb_flag(ev.code);
    if (!flag) {
      continue;
    }
    if (ev.value != 0) {
      *state |= (uint16_t)flag;
    } else {
      *state &= (uint16_t)~flag;
    }
  }

  if (!menu->open && (*state & (VB_KEY_START | VB_KEY_SELECT)) ==
      (VB_KEY_START | VB_KEY_SELECT)) {
    g_stop_requested = 1;
  }
  if (menu->open) {
    *state = 0;
  }
  apply_input_state(*state);
}

static int sleep_until_next_frame(struct timespec *next_frame) {
  struct timespec now;

  next_frame->tv_nsec += 1000000000L / TARGET_FPS;
  while (next_frame->tv_nsec >= 1000000000L) {
    next_frame->tv_nsec -= 1000000000L;
    next_frame->tv_sec++;
  }

  clock_gettime(CLOCK_MONOTONIC, &now);
  if (next_frame->tv_sec < now.tv_sec ||
      (next_frame->tv_sec == now.tv_sec &&
       next_frame->tv_nsec <= now.tv_nsec)) {
    *next_frame = now;
    return 0;
  }

  for (;;) {
    int rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, next_frame, NULL);
    if (rc == 0) {
      return 1;
    }
    if (rc != EINTR || g_stop_requested) {
      return 0;
    }
  }
}

static const char *path_basename(const char *path) {
  const char *base = strrchr(path, '/');
  return base ? base + 1 : path;
}

static void sanitize_save_base(char *name) {
  for (char *p = name; *p; p++) {
    if (*p == '/' || *p == '\\' || *p == ':' || *p == '\'' || *p == '"') {
      *p = '_';
    }
  }
  char *dot = strrchr(name, '.');
  if (dot) {
    *dot = '\0';
  }
  if (name[0] == '\0') {
    strcpy(name, "game");
  }
}

static void configure_paths(const char *rom_path, const char *save_base) {
  const char *home = getenv("HOME");
  if (!home || !*home) {
    home = ".";
  }
  mkdir(home, 0777);
  char save_dir[256];
  snprintf(save_dir, sizeof(save_dir), "%s/saves", home);
  mkdir(save_dir, 0777);

  char base[128];
  snprintf(base, sizeof(base), "%s", save_base && *save_base ? save_base
                                                             : path_basename(rom_path));
  sanitize_save_base(base);

  snprintf(tVBOpt.ROM_PATH, sizeof(tVBOpt.ROM_PATH), "%s", rom_path);
  snprintf(tVBOpt.RAM_PATH, sizeof(tVBOpt.RAM_PATH), "%s/%s.ram", save_dir, base);
  snprintf(tVBOpt.HOME_PATH, sizeof(tVBOpt.HOME_PATH), "%s", home);
}

static int load_rom(void) {
  int ret = v810_load_init();
  if (ret < 0) {
    return ret;
  }
  for (;;) {
    ret = v810_load_step();
    if (ret < 0) {
      return ret;
    }
    if (ret == 100) {
      return 0;
    }
  }
}

static void save_sram_if_needed(void) {
  if (!is_sram || !tVBOpt.RAM_PATH[0]) {
    return;
  }
  FILE *f = fopen(tVBOpt.RAM_PATH, "wb");
  if (!f) {
    fprintf(stderr, "red-viper-a30: SRAM save failed: %s\n", strerror(errno));
    return;
  }
  fwrite(vb_players[0].V810_GAME_RAM.pmemory, 1,
         vb_players[0].V810_GAME_RAM.size, f);
  fclose(f);
}

static rotate_mode_t parse_rotation(const char *value) {
  if (value && strcasecmp(value, "cw") == 0) {
    return ROTATE_CW;
  }
  if (value && (strcasecmp(value, "none") == 0 || strcmp(value, "0") == 0)) {
    return ROTATE_NONE;
  }
  return ROTATE_CCW;
}

static scale_mode_t parse_scale(const char *value) {
  if (value && strcasecmp(value, "stretch") == 0) {
    return SCALE_STRETCH;
  }
  if (value && strcasecmp(value, "integer") == 0) {
    return SCALE_INTEGER;
  }
  return SCALE_FIT;
}

static eye_mode_t parse_eye_mode(const char *value) {
  if (value && strcasecmp(value, "left") == 0) {
    return EYE_LEFT;
  }
  if (value && strcasecmp(value, "right") == 0) {
    return EYE_RIGHT;
  }
  return EYE_BOTH;
}

static int parse_bool_value(const char *value, int default_value) {
  if (!value || !*value) {
    return default_value;
  }
  if (strcasecmp(value, "1") == 0 || strcasecmp(value, "on") == 0 ||
      strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0 ||
      strcasecmp(value, "enabled") == 0) {
    return 1;
  }
  if (strcasecmp(value, "0") == 0 || strcasecmp(value, "off") == 0 ||
      strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0 ||
      strcasecmp(value, "none") == 0 || strcasecmp(value, "disabled") == 0) {
    return 0;
  }
  return default_value;
}

static unsigned parse_unsigned_value(const char *value, unsigned default_value,
                                     unsigned min_value,
                                     unsigned max_value) {
  char *end = NULL;
  unsigned long parsed;

  if (!value || !*value) {
    return default_value;
  }
  parsed = strtoul(value, &end, 10);
  if (end == value || *end != '\0' || parsed < min_value ||
      parsed > max_value) {
    return default_value;
  }
  return (unsigned)parsed;
}

static color_config_t parse_color(const char *value) {
  color_config_t color;
  unsigned r;
  unsigned g;
  unsigned b;
  char tail;

  set_color_preset(&color, COLOR_RED);
  if (!value || !*value) {
    return color;
  }
  if (strcasecmp(value, "red") == 0) {
    set_color_preset(&color, COLOR_RED);
  } else if (strcasecmp(value, "white") == 0 ||
             strcasecmp(value, "gray") == 0 ||
             strcasecmp(value, "grey") == 0) {
    set_color_preset(&color, COLOR_WHITE);
  } else if (strcasecmp(value, "green") == 0) {
    set_color_preset(&color, COLOR_GREEN);
  } else if (strcasecmp(value, "amber") == 0) {
    set_color_preset(&color, COLOR_AMBER);
  } else if (strcasecmp(value, "blue") == 0) {
    set_color_preset(&color, COLOR_BLUE);
  } else if (sscanf(value, "%u,%u,%u%c", &r, &g, &b, &tail) == 3 &&
             r <= 255 && g <= 255 && b <= 255) {
    color.mode = COLOR_CUSTOM;
    color.r = (uint8_t)r;
    color.g = (uint8_t)g;
    color.b = (uint8_t)b;
  } else if (value[0] == '#' && strlen(value) == 7) {
    char hex[3] = {0, 0, 0};
    hex[0] = value[1];
    hex[1] = value[2];
    r = (unsigned)strtoul(hex, NULL, 16);
    hex[0] = value[3];
    hex[1] = value[4];
    g = (unsigned)strtoul(hex, NULL, 16);
    hex[0] = value[5];
    hex[1] = value[6];
    b = (unsigned)strtoul(hex, NULL, 16);
    color.mode = COLOR_CUSTOM;
    color.r = (uint8_t)r;
    color.g = (uint8_t)g;
    color.b = (uint8_t)b;
  }
  return color;
}

static bool audio_enabled_mode(const char *value) {
  if (!value || !*value) {
    return true;
  }
  if (strcasecmp(value, "0") == 0 || strcasecmp(value, "off") == 0 ||
      strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0 ||
      strcasecmp(value, "none") == 0 || strcasecmp(value, "disabled") == 0) {
    return false;
  }
  return true;
}

int main(int argc, char **argv) {
  const char *fb_path = getenv("PLUMOS_A30_RED_VIPER_FB");
  const char *input_path = getenv("PLUMOS_A30_RED_VIPER_INPUT");
  const char *save_base = getenv("PLUMOS_A30_RED_VIPER_SAVE_BASE");
  const char *audio_mode = getenv("PLUMOS_A30_RED_VIPER_AUDIO");
  rotate_mode_t rotation = parse_rotation(getenv("PLUMOS_A30_RED_VIPER_ROTATION"));
  scale_mode_t scale = parse_scale(getenv("PLUMOS_A30_RED_VIPER_SCALE"));
  eye_mode_t eye_mode = parse_eye_mode(getenv("PLUMOS_A30_RED_VIPER_EYE"));
  color_config_t color = parse_color(getenv("PLUMOS_A30_RED_VIPER_COLOR"));
  const char *rom_path = NULL;

  if (!fb_path || !*fb_path) {
    fb_path = "/dev/fb0";
  }
  if (!input_path || !*input_path) {
    input_path = "/dev/input/event3";
  }
  if (!audio_mode || !*audio_mode) {
    audio_mode = "alsa";
  }

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--fb") == 0 && i + 1 < argc) {
      fb_path = argv[++i];
    } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
      input_path = argv[++i];
    } else if (strcmp(argv[i], "--save-base") == 0 && i + 1 < argc) {
      save_base = argv[++i];
    } else if (strcmp(argv[i], "--rotation") == 0 && i + 1 < argc) {
      rotation = parse_rotation(argv[++i]);
    } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
      scale = parse_scale(argv[++i]);
    } else if (strcmp(argv[i], "--eye") == 0 && i + 1 < argc) {
      eye_mode = parse_eye_mode(argv[++i]);
    } else if (strcmp(argv[i], "--color") == 0 && i + 1 < argc) {
      color = parse_color(argv[++i]);
    } else if (strcmp(argv[i], "--audio") == 0 && i + 1 < argc) {
      audio_mode = argv[++i];
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(stdout);
      return 0;
    } else if (argv[i][0] == '-') {
      usage(stderr);
      return 2;
    } else {
      rom_path = argv[i];
    }
  }

  if (!rom_path) {
    usage(stderr);
    return 2;
  }
  bool audio_enabled = audio_enabled_mode(audio_mode);
  setenv("PLUMOS_A30_RED_VIPER_AUDIO", audio_enabled ? audio_mode : "off", 1);

  red_viper_settings_t settings;
  memset(&settings, 0, sizeof(settings));
  settings.audio_enabled = audio_enabled ? 1 : 0;
  settings.rotation = rotation;
  settings.audio_latency_us =
      parse_unsigned_value(getenv("PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US"),
                           160000, 120000, 500000);
  settings.audio_prebuffer_chunks =
      parse_unsigned_value(getenv("PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS"),
                           6, 0, 8);
  settings.audio_queue_chunks =
      parse_unsigned_value(getenv("PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS"),
                           8, 2, 32);
  const char *cpu_policy = getenv("PLUMOS_A30_RED_VIPER_CPU_POLICY");
  if (cpu_policy && strcasecmp(cpu_policy, "performance") == 0) {
    snprintf(settings.cpu_policy, sizeof(settings.cpu_policy), "performance");
  } else {
    snprintf(settings.cpu_policy, sizeof(settings.cpu_policy), "fixed");
  }
  settings.cpu_freq =
      parse_unsigned_value(getenv("PLUMOS_A30_RED_VIPER_CPU_FREQ"), 648000,
                           100000, 2000000);
  settings.cpu_cores =
      parse_unsigned_value(getenv("PLUMOS_A30_RED_VIPER_CPU_CORES"), 2, 2, 4);
  if (settings.cpu_cores != 4) {
    settings.cpu_cores = 2;
  }

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  signal(SIGHUP, on_signal);

  fb_ctx_t fb;
  if (fb_open(&fb, fb_path, rotation, scale, eye_mode, color) < 0) {
    fb_close(&fb);
    return 1;
  }
  if (!parse_bool_value(getenv("PLUMOS_A30_RED_VIPER_WAIT_VSYNC"), 1)) {
    fb.wait_vsync = 0;
  }
  fb_clear(&fb);

  int input_fd = open_input(input_path);
  uint16_t input_state = 0;
  menu_ctx_t menu = {0, MENU_RESUME, 0};

  setDefaults();
  tVBOpt.SOUND = settings.audio_enabled ? 1 : 0;
  tVBOpt.FRMSKIP =
      (int)parse_unsigned_value(getenv("PLUMOS_A30_RED_VIPER_FRAME_SKIP"), 0,
                                0, 3);
  tVBOpt.FASTFORWARD =
      parse_bool_value(getenv("PLUMOS_A30_RED_VIPER_FAST_FORWARD"), 0);
  tVBOpt.FF_TOGGLE =
      parse_bool_value(getenv("PLUMOS_A30_RED_VIPER_FF_TOGGLE"), 0);
  tVBOpt.VIP_OVERCLOCK =
      parse_bool_value(getenv("PLUMOS_A30_RED_VIPER_VIP_OVERCLOCK"), 0);
  tVBOpt.PERF_INFO =
      parse_bool_value(getenv("PLUMOS_A30_RED_VIPER_PERF_INFO"), 0);
  tVBOpt.TINT = color_to_tint(&fb.color);
  tVBOpt.RENDERMODE = RM_CPUONLY;
  tVBOpt.VSYNC = true;
  configure_paths(rom_path, save_base);

  v810_init();
  replay_init();
#if DRC_AVAILABLE
  drc_init();
#endif
  video_init();

  int ret = load_rom();
  if (ret < 0) {
    fprintf(stderr, "red-viper-a30: ROM load failed: %d\n", ret);
    fb_close(&fb);
    if (input_fd >= 0) {
      close(input_fd);
    }
    return 1;
  }
  tVBOpt.RENDERMODE = RM_CPUONLY;
  sound_init();

  struct timespec next_frame;
  clock_gettime(CLOCK_MONOTONIC, &next_frame);
  unsigned frame_skip_phase = 0;
  perf_stats_t perf_stats;
  perf_stats_reset(&perf_stats);

  while (!g_stop_requested) {
    int rendered_this_frame = 0;
    int presented_this_frame = 0;
    int late_this_frame = 0;

    poll_input(input_fd, &input_state, &menu, &fb, &settings);
    if (settings.reset_requested) {
      save_sram_if_needed();
      v810_reset();
      tVBOpt.RENDERMODE = RM_CPUONLY;
      sound_reset();
      input_state = 0;
      apply_input_state(input_state);
      frame_skip_phase = 0;
      settings.reset_requested = 0;
      clock_gettime(CLOCK_MONOTONIC, &next_frame);
      perf_stats_reset(&perf_stats);
    }
    int displayed_fb = vb_state->tVIPREG.tDisplayedFB & 1;
    if (vb_state->tVIPREG.tFrame == 0 && !vb_state->tVIPREG.drawing &&
        (vb_state->tVIPREG.DPCTRL & DISP)) {
      unsigned frame_skip_divisor = (unsigned)tVBOpt.FRMSKIP + 1u;
      int should_present = menu.open || frame_skip_phase == 0;
      if (vb_state->tVIPREG.XPCTRL & XPEN) {
        render_vip_frame(!displayed_fb);
        rendered_this_frame = 1;
      }
      if (should_present) {
        fb_blit_vb(&fb, displayed_fb, &menu, &settings, &perf_stats);
        presented_this_frame = 1;
      }
      frame_skip_phase = (frame_skip_phase + 1u) % frame_skip_divisor;
    }

    ret = v810_run();
    if (ret != 0) {
      fprintf(stderr, "red-viper-a30: emulation error: %d\n", ret);
      break;
    }
    poll_input(input_fd, &input_state, &menu, &fb, &settings);
    if (tVBOpt.FASTFORWARD && !menu.open) {
      clock_gettime(CLOCK_MONOTONIC, &next_frame);
    } else {
      late_this_frame = !sleep_until_next_frame(&next_frame);
    }
    perf_stats_update(&perf_stats, rendered_this_frame, presented_this_frame,
                      late_this_frame);
  }

  save_sram_if_needed();
  sound_close();
#if DRC_AVAILABLE
  drc_exit();
#endif
  v810_exit();
  fb_clear(&fb);
  fb_close(&fb);
  if (input_fd >= 0) {
    close(input_fd);
  }

  return ret == 0 ? 0 : 1;
}
