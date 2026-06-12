#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
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

typedef struct {
  int fd;
  uint8_t *pixels;
  size_t map_size;
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
  rotate_mode_t rotation;
  scale_mode_t scale;
  eye_mode_t eye_mode;
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

static volatile sig_atomic_t g_stop_requested;

static void on_signal(int sig) {
  (void)sig;
  g_stop_requested = 1;
}

static void usage(FILE *out) {
  fprintf(out,
          "Usage: red-viper-a30 [--fb PATH] [--input PATH] [--save-base NAME] "
          "[--rotation ccw|cw|none] [--scale fit|stretch|integer] "
          "[--eye left|right|both] [--audio alsa|off] ROM\n");
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
                   scale_mode_t scale, eye_mode_t eye_mode) {
  memset(fb, 0, sizeof(*fb));
  fb->fd = -1;
  fb->rotation = rotation;
  fb->scale = scale;
  fb->eye_mode = eye_mode;

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
    int red = brt[i] * 2;
    if (red > 255) {
      red = 255;
    }
    palette[i] = pack_color(fb, (uint8_t)red, 0, 0);
  }
}

static int vb_framebuffer_pixel(const uint16_t *vb_fb, int sx, int sy) {
  int word_y = sy >> 3;
  int shift = (sy & 7) * 2;
  uint16_t word = vb_fb[sx * 32 + word_y];
  return (word >> shift) & 3;
}

static void fb_blit_vb(fb_ctx_t *fb, int fb_index) {
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
  fb_clear_page(fb, target_yoffset);
  for (int y = 0; y < fb->out_h; y++) {
    int sy = fb->src_y[y];
    for (int x = 0; x < fb->out_w; x++) {
      int sx = fb->src_x[x];
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
      if (colour != 0) {
        put_logical_pixel_page(fb, fb->out_x + x, fb->out_y + y,
                               target_yoffset, palette[colour]);
      }
    }
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

static void poll_input(int input_fd, uint16_t *state) {
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
    if (ev.code == KEY_ESC && ev.value != 0) {
      g_stop_requested = 1;
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

  if ((*state & (VB_KEY_START | VB_KEY_SELECT)) ==
      (VB_KEY_START | VB_KEY_SELECT)) {
    g_stop_requested = 1;
  }
  apply_input_state(*state);
}

static void sleep_until_next_frame(struct timespec *next_frame) {
  next_frame->tv_nsec += 1000000000L / TARGET_FPS;
  while (next_frame->tv_nsec >= 1000000000L) {
    next_frame->tv_nsec -= 1000000000L;
    next_frame->tv_sec++;
  }

  for (;;) {
    int rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, next_frame, NULL);
    if (rc == 0) {
      return;
    }
    if (rc != EINTR || g_stop_requested) {
      return;
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

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  signal(SIGHUP, on_signal);

  fb_ctx_t fb;
  if (fb_open(&fb, fb_path, rotation, scale, eye_mode) < 0) {
    fb_close(&fb);
    return 1;
  }
  fb_clear(&fb);

  int input_fd = open_input(input_path);
  uint16_t input_state = 0;

  setDefaults();
  tVBOpt.SOUND = audio_enabled ? 1 : 0;
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

  while (!g_stop_requested) {
    poll_input(input_fd, &input_state);
    int displayed_fb = vb_state->tVIPREG.tDisplayedFB & 1;
    if (vb_state->tVIPREG.tFrame == 0 && !vb_state->tVIPREG.drawing &&
        (vb_state->tVIPREG.DPCTRL & DISP)) {
      if (vb_state->tVIPREG.XPCTRL & XPEN) {
        render_vip_frame(!displayed_fb);
      }
      fb_blit_vb(&fb, displayed_fb);
    }

    ret = v810_run();
    if (ret != 0) {
      fprintf(stderr, "red-viper-a30: emulation error: %d\n", ret);
      break;
    }
    poll_input(input_fd, &input_state);
    sleep_until_next_frame(&next_frame);
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
