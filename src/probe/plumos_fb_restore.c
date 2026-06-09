#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/fb.h>

static void usage(const char *argv0) {
  printf("Usage: %s [--fb PATH] [--quiet]\n", argv0);
}

static void print_var(const char *label, const struct fb_var_screeninfo *var,
                      const struct fb_fix_screeninfo *fix) {
  printf("%s xres=%u yres=%u virtual=%ux%u offset=%u,%u bpp=%u line=%u "
         "rgba=%u/%u,%u/%u,%u/%u,%u/%u\n",
         label, var->xres, var->yres, var->xres_virtual, var->yres_virtual,
         var->xoffset, var->yoffset, var->bits_per_pixel, fix->line_length,
         var->red.length, var->red.offset, var->green.length, var->green.offset,
         var->blue.length, var->blue.offset, var->transp.length, var->transp.offset);
}

static void set_component(struct fb_bitfield *field, unsigned int offset,
                          unsigned int length) {
  field->offset = offset;
  field->length = length;
  field->msb_right = 0;
}

static void set_a30_mode(struct fb_var_screeninfo *var, unsigned int yres_virtual) {
  var->xres = 480;
  var->yres = 640;
  var->xres_virtual = 480;
  var->yres_virtual = yres_virtual;
  var->xoffset = 0;
  var->yoffset = 0;
  var->bits_per_pixel = 32;
  var->grayscale = 0;
  var->nonstd = 0;
  var->activate = FB_ACTIVATE_NOW;
  set_component(&var->red, 16, 8);
  set_component(&var->green, 8, 8);
  set_component(&var->blue, 0, 8);
  set_component(&var->transp, 24, 8);
}

static int put_and_pan(int fd, struct fb_var_screeninfo *var, const char *label) {
  if (ioctl(fd, FBIOPUT_VSCREENINFO, var) != 0) {
    fprintf(stderr, "error: %s framebuffer mode: %s\n", label, strerror(errno));
    return 0;
  }

  var->xoffset = 0;
  var->yoffset = 0;
  var->activate = FB_ACTIVATE_NOW;
  if (ioctl(fd, FBIOPAN_DISPLAY, var) != 0) {
    fprintf(stderr, "warning: %s framebuffer display pan: %s\n", label, strerror(errno));
  }
  return 1;
}

int main(int argc, char **argv) {
  const char *fb_path = "/dev/fb0";
  int quiet = 0;
  int fd;
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;

  for (int i = 1; i < argc; i++) {
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

  fd = open(fb_path, O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "error: open %s: %s\n", fb_path, strerror(errno));
    return 1;
  }

  memset(&var, 0, sizeof(var));
  memset(&fix, 0, sizeof(fix));
  if (ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0 ||
      ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0) {
    fprintf(stderr, "error: get framebuffer info: %s\n", strerror(errno));
    close(fd);
    return 1;
  }

  if (!quiet) {
    print_var("before", &var, &fix);
  }

  set_a30_mode(&var, 640);
  if (!put_and_pan(fd, &var, "reset")) {
    close(fd);
    return 1;
  }

  set_a30_mode(&var, 1280);
  if (!put_and_pan(fd, &var, "restore")) {
    close(fd);
    return 1;
  }

  memset(&var, 0, sizeof(var));
  memset(&fix, 0, sizeof(fix));
  if (ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0 ||
      ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0) {
    fprintf(stderr, "error: get restored framebuffer info: %s\n", strerror(errno));
    close(fd);
    return 1;
  }

  if (!quiet) {
    print_var("after", &var, &fix);
  }

  close(fd);
  return 0;
}
