#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef void *EGLNativeDisplayType;
typedef void *EGLNativeWindowType;

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;

#define EGL_FALSE 0
#define EGL_TRUE 1
#define EGL_NONE 0x3038
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)
#define EGL_SUCCESS 0x3000
#define EGL_NOT_INITIALIZED 0x3001
#define EGL_BAD_ACCESS 0x3002
#define EGL_BAD_ALLOC 0x3003
#define EGL_BAD_ATTRIBUTE 0x3004
#define EGL_BAD_CONFIG 0x3005
#define EGL_BAD_CONTEXT 0x3006
#define EGL_BAD_CURRENT_SURFACE 0x3007
#define EGL_BAD_DISPLAY 0x3008
#define EGL_BAD_MATCH 0x3009
#define EGL_BAD_NATIVE_PIXMAP 0x300a
#define EGL_BAD_NATIVE_WINDOW 0x300b
#define EGL_BAD_PARAMETER 0x300c
#define EGL_BAD_SURFACE 0x300d
#define EGL_CONTEXT_LOST 0x300e
#define EGL_BUFFER_SIZE 0x3020
#define EGL_ALPHA_SIZE 0x3021
#define EGL_BLUE_SIZE 0x3022
#define EGL_GREEN_SIZE 0x3023
#define EGL_RED_SIZE 0x3024
#define EGL_DEPTH_SIZE 0x3025
#define EGL_NATIVE_VISUAL_ID 0x302e
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_VENDOR 0x3053
#define EGL_VERSION 0x3054
#define EGL_EXTENSIONS 0x3055
#define EGL_CLIENT_APIS 0x308d
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_OPENGL_ES_API 0x30a0
#define EGL_OPENGL_ES2_BIT 0x0004

#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_UNSIGNED_BYTE 0x1401
#define GL_RGBA 0x1908
#define GL_VENDOR 0x1f00
#define GL_RENDERER 0x1f01
#define GL_VERSION 0x1f02

struct egl_api {
  void *handle;
  EGLDisplay (*GetDisplay)(EGLNativeDisplayType display_id);
  EGLBoolean (*Initialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
  const char *(*QueryString)(EGLDisplay dpy, EGLint name);
  EGLBoolean (*ChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs,
                             EGLint config_size, EGLint *num_config);
  EGLBoolean (*GetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute,
                                EGLint *value);
  EGLBoolean (*BindAPI)(EGLenum api);
  EGLSurface (*CreateWindowSurface)(EGLDisplay dpy, EGLConfig config,
                                    EGLNativeWindowType win, const EGLint *attrib_list);
  EGLContext (*CreateContext)(EGLDisplay dpy, EGLConfig config, EGLContext share_context,
                              const EGLint *attrib_list);
  EGLBoolean (*MakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                            EGLContext ctx);
  EGLBoolean (*SwapBuffers)(EGLDisplay dpy, EGLSurface surface);
  EGLBoolean (*SwapInterval)(EGLDisplay dpy, EGLint interval);
  EGLBoolean (*QuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute,
                             EGLint *value);
  EGLint (*GetError)(void);
  EGLBoolean (*DestroyContext)(EGLDisplay dpy, EGLContext ctx);
  EGLBoolean (*DestroySurface)(EGLDisplay dpy, EGLSurface surface);
  EGLBoolean (*Terminate)(EGLDisplay dpy);
};

struct gl_api {
  void *handle;
  const GLubyte *(*GetString)(GLenum name);
  void (*Viewport)(GLint x, GLint y, GLsizei width, GLsizei height);
  void (*ClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
  void (*Clear)(GLbitfield mask);
  void (*Finish)(void);
  void (*ReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                     GLenum type, void *pixels);
};

struct fbdev_window_u16 {
  uint16_t width;
  uint16_t height;
};

struct fbdev_window_u32 {
  uint32_t width;
  uint32_t height;
};

struct native_window_storage {
  struct fbdev_window_u16 win16;
  struct fbdev_window_u32 win32;
};

struct config {
  const char *fb_path;
  const char *egl_lib;
  const char *gles_lib;
  const char *window_mode;
  int run_ms;
  int frames;
};

static const char *egl_error_name(EGLint err) {
  switch (err) {
  case EGL_SUCCESS:
    return "EGL_SUCCESS";
  case EGL_NOT_INITIALIZED:
    return "EGL_NOT_INITIALIZED";
  case EGL_BAD_ACCESS:
    return "EGL_BAD_ACCESS";
  case EGL_BAD_ALLOC:
    return "EGL_BAD_ALLOC";
  case EGL_BAD_ATTRIBUTE:
    return "EGL_BAD_ATTRIBUTE";
  case EGL_BAD_CONFIG:
    return "EGL_BAD_CONFIG";
  case EGL_BAD_CONTEXT:
    return "EGL_BAD_CONTEXT";
  case EGL_BAD_CURRENT_SURFACE:
    return "EGL_BAD_CURRENT_SURFACE";
  case EGL_BAD_DISPLAY:
    return "EGL_BAD_DISPLAY";
  case EGL_BAD_MATCH:
    return "EGL_BAD_MATCH";
  case EGL_BAD_NATIVE_PIXMAP:
    return "EGL_BAD_NATIVE_PIXMAP";
  case EGL_BAD_NATIVE_WINDOW:
    return "EGL_BAD_NATIVE_WINDOW";
  case EGL_BAD_PARAMETER:
    return "EGL_BAD_PARAMETER";
  case EGL_BAD_SURFACE:
    return "EGL_BAD_SURFACE";
  case EGL_CONTEXT_LOST:
    return "EGL_CONTEXT_LOST";
  default:
    return "EGL_UNKNOWN";
  }
}

static int load_symbol(void *handle, const char *name, void *fn_out) {
  void *symbol;

  dlerror();
  symbol = dlsym(handle, name);
  if (!symbol) {
    const char *err = dlerror();
    printf("dlsym name=%s ok=no error=\"%s\"\n", name, err ? err : "-");
    return 0;
  }
  memcpy(fn_out, &symbol, sizeof(symbol));
  return 1;
}

#define LOAD_EGL(api, field, symbol)                                                               \
  do {                                                                                             \
    if (!load_symbol((api)->handle, (symbol), &(api)->field)) {                                    \
      return 0;                                                                                    \
    }                                                                                              \
  } while (0)

#define LOAD_GL(api, field, symbol)                                                                \
  do {                                                                                             \
    if (!load_symbol((api)->handle, (symbol), &(api)->field)) {                                    \
      return 0;                                                                                    \
    }                                                                                              \
  } while (0)

static int load_egl(struct egl_api *api, const char *path) {
  const char *err;

  memset(api, 0, sizeof(*api));
  api->handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
  err = api->handle ? "" : dlerror();
  printf("dlopen egl=\"%s\" ok=%s error=\"%s\"\n", path, api->handle ? "yes" : "no",
         err ? err : "-");
  if (!api->handle) {
    return 0;
  }

  LOAD_EGL(api, GetDisplay, "eglGetDisplay");
  LOAD_EGL(api, Initialize, "eglInitialize");
  LOAD_EGL(api, QueryString, "eglQueryString");
  LOAD_EGL(api, ChooseConfig, "eglChooseConfig");
  LOAD_EGL(api, GetConfigAttrib, "eglGetConfigAttrib");
  LOAD_EGL(api, BindAPI, "eglBindAPI");
  LOAD_EGL(api, CreateWindowSurface, "eglCreateWindowSurface");
  LOAD_EGL(api, CreateContext, "eglCreateContext");
  LOAD_EGL(api, MakeCurrent, "eglMakeCurrent");
  LOAD_EGL(api, SwapBuffers, "eglSwapBuffers");
  LOAD_EGL(api, SwapInterval, "eglSwapInterval");
  LOAD_EGL(api, QuerySurface, "eglQuerySurface");
  LOAD_EGL(api, GetError, "eglGetError");
  LOAD_EGL(api, DestroyContext, "eglDestroyContext");
  LOAD_EGL(api, DestroySurface, "eglDestroySurface");
  LOAD_EGL(api, Terminate, "eglTerminate");
  return 1;
}

static int load_gl(struct gl_api *api, const char *path) {
  const char *err;

  memset(api, 0, sizeof(*api));
  api->handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
  err = api->handle ? "" : dlerror();
  printf("dlopen gles=\"%s\" ok=%s error=\"%s\"\n", path, api->handle ? "yes" : "no",
         err ? err : "-");
  if (!api->handle) {
    return 0;
  }

  LOAD_GL(api, GetString, "glGetString");
  LOAD_GL(api, Viewport, "glViewport");
  LOAD_GL(api, ClearColor, "glClearColor");
  LOAD_GL(api, Clear, "glClear");
  LOAD_GL(api, Finish, "glFinish");
  LOAD_GL(api, ReadPixels, "glReadPixels");
  return 1;
}

static void print_usage(const char *argv0) {
  printf("Usage: %s [OPTIONS]\n", argv0);
  printf("\n");
  printf("Probe A30 fbdev + Mali EGL/GLES presentation without linking to stock SDL.\n");
  printf("\n");
  printf("Options:\n");
  printf("  --fb PATH           Framebuffer path. Default: /dev/fb0\n");
  printf("  --egl-lib PATH      EGL library to dlopen. Default: /usr/lib/libEGL.so\n");
  printf("  --gles-lib PATH     GLESv2 library to dlopen. Default: /usr/lib/libGLESv2.so\n");
  printf("  --window-mode MODE  auto, u16, u32, null. Default: auto\n");
  printf("  --run-ms MS         Draw/swap duration. Default: 500\n");
  printf("  --frames N          Max frames to draw. Default: 30\n");
}

static int parse_int_arg(const char *name, const char *value, int *out) {
  char *end = NULL;
  long parsed;

  errno = 0;
  parsed = strtol(value, &end, 10);
  if (errno || !end || *end != '\0' || parsed < 0 || parsed > 1000000) {
    printf("error invalid %s: %s\n", name, value);
    return 0;
  }
  *out = (int)parsed;
  return 1;
}

static int parse_args(int argc, char **argv, struct config *cfg) {
  int i;

  cfg->fb_path = "/dev/fb0";
  cfg->egl_lib = "/usr/lib/libEGL.so";
  cfg->gles_lib = "/usr/lib/libGLESv2.so";
  cfg->window_mode = "auto";
  cfg->run_ms = 500;
  cfg->frames = 30;

  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--fb") == 0 && i + 1 < argc) {
      cfg->fb_path = argv[++i];
    } else if (strcmp(argv[i], "--egl-lib") == 0 && i + 1 < argc) {
      cfg->egl_lib = argv[++i];
    } else if (strcmp(argv[i], "--gles-lib") == 0 && i + 1 < argc) {
      cfg->gles_lib = argv[++i];
    } else if (strcmp(argv[i], "--window-mode") == 0 && i + 1 < argc) {
      cfg->window_mode = argv[++i];
    } else if (strcmp(argv[i], "--run-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--run-ms", argv[++i], &cfg->run_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--frames", argv[++i], &cfg->frames)) {
        return 0;
      }
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      exit(0);
    } else {
      print_usage(argv[0]);
      return 0;
    }
  }

  if (strcmp(cfg->window_mode, "auto") != 0 && strcmp(cfg->window_mode, "u16") != 0 &&
      strcmp(cfg->window_mode, "u32") != 0 && strcmp(cfg->window_mode, "null") != 0) {
    printf("error invalid --window-mode: %s\n", cfg->window_mode);
    return 0;
  }

  return 1;
}

static int64_t monotonic_ms(void) {
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void sleep_ms(int ms) {
  struct timespec ts;

  if (ms <= 0) {
    return;
  }
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

static int open_fb(const char *path, int *fd_out, struct fb_var_screeninfo *var_out,
                   struct fb_fix_screeninfo *fix_out) {
  int fd;

  fd = open(path, O_RDWR | O_CLOEXEC);
  printf("fb path=%s open=%s error=\"%s\"\n", path, fd >= 0 ? "yes" : "no",
         fd >= 0 ? "" : strerror(errno));
  if (fd < 0) {
    return 0;
  }
  if (ioctl(fd, FBIOGET_VSCREENINFO, var_out) != 0) {
    printf("fb var=no error=\"%s\"\n", strerror(errno));
    close(fd);
    return 0;
  }
  if (ioctl(fd, FBIOGET_FSCREENINFO, fix_out) != 0) {
    printf("fb fix=no error=\"%s\"\n", strerror(errno));
    close(fd);
    return 0;
  }
  printf("fb var=yes xres=%u yres=%u virtual=%ux%u offset=%u,%u bpp=%u visual=%u\n",
         var_out->xres, var_out->yres, var_out->xres_virtual, var_out->yres_virtual,
         var_out->xoffset, var_out->yoffset, var_out->bits_per_pixel, fix_out->visual);
  printf("fb fix=yes id=\"%s\" smem_len=%lu line_length=%u type=%u visual=%u\n", fix_out->id,
         (unsigned long)fix_out->smem_len, fix_out->line_length, fix_out->type,
         fix_out->visual);
  *fd_out = fd;
  return 1;
}

static int choose_config(struct egl_api *egl, EGLDisplay display, EGLConfig *config_out) {
  const EGLint rgba8888_attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 0,
    EGL_DEPTH_SIZE, 0, EGL_NONE,
  };
  const EGLint rgb565_attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5, EGL_ALPHA_SIZE, 0,
    EGL_DEPTH_SIZE, 0, EGL_NONE,
  };
  EGLConfig config = NULL;
  EGLint count = 0;
  EGLint value = 0;

  if (egl->ChooseConfig(display, rgba8888_attribs, &config, 1, &count) == EGL_TRUE &&
      count > 0) {
    printf("egl choose_config=rgba8888 yes count=%d\n", count);
  } else if (egl->ChooseConfig(display, rgb565_attribs, &config, 1, &count) == EGL_TRUE &&
             count > 0) {
    printf("egl choose_config=rgb565 yes count=%d\n", count);
  } else {
    EGLint err = egl->GetError();
    printf("egl choose_config=no error=0x%04x %s count=%d\n", err, egl_error_name(err),
           count);
    return 0;
  }

  if (egl->GetConfigAttrib(display, config, EGL_RED_SIZE, &value) == EGL_TRUE) {
    printf("egl config red=%d", value);
    if (egl->GetConfigAttrib(display, config, EGL_GREEN_SIZE, &value) == EGL_TRUE) {
      printf(" green=%d", value);
    }
    if (egl->GetConfigAttrib(display, config, EGL_BLUE_SIZE, &value) == EGL_TRUE) {
      printf(" blue=%d", value);
    }
    if (egl->GetConfigAttrib(display, config, EGL_ALPHA_SIZE, &value) == EGL_TRUE) {
      printf(" alpha=%d", value);
    }
    if (egl->GetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &value) == EGL_TRUE) {
      printf(" native_visual=0x%x", value);
    }
    printf("\n");
  }

  *config_out = config;
  return 1;
}

static EGLSurface try_surface(struct egl_api *egl, EGLDisplay display, EGLConfig config,
                              struct native_window_storage *storage, const char *mode,
                              uint32_t width, uint32_t height) {
  EGLNativeWindowType native = NULL;
  EGLSurface surface;
  EGLint err;

  if (strcmp(mode, "u16") == 0) {
    storage->win16.width = (uint16_t)width;
    storage->win16.height = (uint16_t)height;
    native = (EGLNativeWindowType)&storage->win16;
  } else if (strcmp(mode, "u32") == 0) {
    storage->win32.width = width;
    storage->win32.height = height;
    native = (EGLNativeWindowType)&storage->win32;
  } else if (strcmp(mode, "null") == 0) {
    native = NULL;
  } else {
    printf("egl surface mode=%s create=no error=\"internal invalid mode\"\n", mode);
    return EGL_NO_SURFACE;
  }

  surface = egl->CreateWindowSurface(display, config, native, NULL);
  if (surface == EGL_NO_SURFACE) {
    err = egl->GetError();
    printf("egl surface mode=%s create=no native=%p error=0x%04x %s\n", mode, native, err,
           egl_error_name(err));
    return EGL_NO_SURFACE;
  }
  printf("egl surface mode=%s create=yes native=%p surface=%p\n", mode, native, surface);
  return surface;
}

static EGLSurface create_surface(struct egl_api *egl, EGLDisplay display, EGLConfig config,
                                 struct native_window_storage *storage, const char *mode,
                                 uint32_t width, uint32_t height, const char **mode_used) {
  const char *modes[] = { "null", "u16", "u32" };
  size_t i;
  EGLSurface surface;

  if (strcmp(mode, "auto") != 0) {
    surface = try_surface(egl, display, config, storage, mode, width, height);
    if (surface != EGL_NO_SURFACE) {
      *mode_used = mode;
    }
    return surface;
  }

  for (i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
    surface = try_surface(egl, display, config, storage, modes[i], width, height);
    if (surface != EGL_NO_SURFACE) {
      *mode_used = modes[i];
      return surface;
    }
  }
  return EGL_NO_SURFACE;
}

static int draw_frames(struct egl_api *egl, struct gl_api *gl, EGLDisplay display,
                       EGLSurface surface, int width, int height, int run_ms, int max_frames) {
  int frames = 0;
  int64_t start = monotonic_ms();
  int swap_ok = 1;

  if (max_frames <= 0) {
    max_frames = 1;
  }

  while (frames < max_frames) {
    GLfloat t = (GLfloat)(frames % 60) / 60.0f;
    EGLBoolean ok;

    gl->Viewport(0, 0, width, height);
    gl->ClearColor(0.05f + t * 0.6f, 0.12f, 0.30f + (1.0f - t) * 0.4f, 1.0f);
    gl->Clear(GL_COLOR_BUFFER_BIT);
    gl->Finish();
    ok = egl->SwapBuffers(display, surface);
    if (ok != EGL_TRUE) {
      EGLint err = egl->GetError();
      printf("egl swap frame=%d ok=no error=0x%04x %s\n", frames, err,
             egl_error_name(err));
      swap_ok = 0;
      break;
    }
    ++frames;
    if (run_ms > 0 && monotonic_ms() - start >= run_ms) {
      break;
    }
    sleep_ms(16);
  }

  printf("draw frames=%d swap_ok=%s elapsed_ms=%lld\n", frames, swap_ok ? "yes" : "no",
         (long long)(monotonic_ms() - start));
  return swap_ok && frames > 0;
}

int main(int argc, char **argv) {
  struct config cfg;
  struct fb_var_screeninfo fb_var;
  struct fb_fix_screeninfo fb_fix;
  struct egl_api egl = { 0 };
  struct gl_api gl = { 0 };
  struct native_window_storage native_storage;
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLConfig egl_config = NULL;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  EGLint major = 0;
  EGLint minor = 0;
  EGLint err = EGL_SUCCESS;
  EGLint width = 0;
  EGLint height = 0;
  EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  const char *mode_used = "-";
  int fb_fd = -1;
  int ok = 0;
  int draw_ok = 0;
  GLubyte pixel[4] = { 0, 0, 0, 0 };

  if (!parse_args(argc, argv, &cfg)) {
    return 2;
  }

  printf("plumOS Mali EGL probe\n");
  printf("fb=%s egl_lib=%s gles_lib=%s window_mode=%s run_ms=%d frames=%d\n", cfg.fb_path,
         cfg.egl_lib, cfg.gles_lib, cfg.window_mode, cfg.run_ms, cfg.frames);

  if (!open_fb(cfg.fb_path, &fb_fd, &fb_var, &fb_fix)) {
    goto done;
  }
  if (!load_egl(&egl, cfg.egl_lib) || !load_gl(&gl, cfg.gles_lib)) {
    goto done;
  }

  display = egl.GetDisplay(EGL_DEFAULT_DISPLAY);
  printf("egl get_display default=%p ok=%s\n", display, display != EGL_NO_DISPLAY ? "yes" : "no");
  if (display == EGL_NO_DISPLAY) {
    err = egl.GetError();
    printf("egl get_display error=0x%04x %s\n", err, egl_error_name(err));
    goto done;
  }
  if (egl.Initialize(display, &major, &minor) != EGL_TRUE) {
    err = egl.GetError();
    printf("egl initialize=no error=0x%04x %s\n", err, egl_error_name(err));
    goto done;
  }
  printf("egl initialize=yes version=%d.%d\n", major, minor);
  printf("egl vendor=\"%s\"\n", egl.QueryString(display, EGL_VENDOR));
  printf("egl version=\"%s\"\n", egl.QueryString(display, EGL_VERSION));
  printf("egl client_apis=\"%s\"\n", egl.QueryString(display, EGL_CLIENT_APIS));

  if (egl.BindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
    err = egl.GetError();
    printf("egl bind_api=opengles no error=0x%04x %s\n", err, egl_error_name(err));
    goto done;
  }
  printf("egl bind_api=opengles yes\n");

  if (!choose_config(&egl, display, &egl_config)) {
    goto done;
  }
  memset(&native_storage, 0, sizeof(native_storage));
  surface = create_surface(&egl, display, egl_config, &native_storage, cfg.window_mode,
                           fb_var.xres, fb_var.yres, &mode_used);
  if (surface == EGL_NO_SURFACE) {
    goto done;
  }
  if (egl.QuerySurface(display, surface, EGL_WIDTH, &width) == EGL_TRUE &&
      egl.QuerySurface(display, surface, EGL_HEIGHT, &height) == EGL_TRUE) {
    printf("egl surface_size=%dx%d mode_used=%s\n", width, height, mode_used);
  } else {
    width = (EGLint)fb_var.xres;
    height = (EGLint)fb_var.yres;
    printf("egl surface_size=query_failed fallback=%dx%d mode_used=%s\n", width, height,
           mode_used);
  }

  context = egl.CreateContext(display, egl_config, EGL_NO_CONTEXT, context_attribs);
  if (context == EGL_NO_CONTEXT) {
    err = egl.GetError();
    printf("egl context es2=no error=0x%04x %s\n", err, egl_error_name(err));
    goto done;
  }
  printf("egl context es2=yes context=%p\n", context);

  if (egl.MakeCurrent(display, surface, surface, context) != EGL_TRUE) {
    err = egl.GetError();
    printf("egl make_current=no error=0x%04x %s\n", err, egl_error_name(err));
    goto done;
  }
  printf("egl make_current=yes\n");
  egl.SwapInterval(display, 0);

  printf("gl vendor=\"%s\"\n", gl.GetString(GL_VENDOR));
  printf("gl renderer=\"%s\"\n", gl.GetString(GL_RENDERER));
  printf("gl version=\"%s\"\n", gl.GetString(GL_VERSION));

  draw_ok = draw_frames(&egl, &gl, display, surface, width, height, cfg.run_ms, cfg.frames);
  gl.ReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  printf("gl readpixels rgba=%02x%02x%02x%02x\n", pixel[0], pixel[1], pixel[2], pixel[3]);

  ok = draw_ok;

done:
  if (display != EGL_NO_DISPLAY) {
    if (context != EGL_NO_CONTEXT) {
      egl.MakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      egl.DestroyContext(display, context);
    }
    if (surface != EGL_NO_SURFACE) {
      egl.DestroySurface(display, surface);
    }
    egl.Terminate(display);
  }
  if (gl.handle) {
    dlclose(gl.handle);
  }
  if (egl.handle) {
    dlclose(egl.handle);
  }
  if (fb_fd >= 0) {
    close(fb_fd);
  }

  printf("result=%s\n", ok ? "mali_egl_present_ok" : "mali_egl_present_failed");
  return ok ? 0 : 1;
}
