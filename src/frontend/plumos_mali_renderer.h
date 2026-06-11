#ifndef PLUMOS_MALI_RENDERER_H
#define PLUMOS_MALI_RENDERER_H

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <linux/fb.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef PLUMOS_MALI_RENDER_LINE_MAX
#define PLUMOS_MALI_RENDER_LINE_MAX 512
#endif

#define PLUMOS_MALI_SETTING_FLASH_MARKER "@{F:"

#ifdef PLUMOS_ENABLE_MALI_FREETYPE
#ifndef PLUMOS_MALI_FT_ADVANCE_CACHE_SIZE
#define PLUMOS_MALI_FT_ADVANCE_CACHE_SIZE 512
#endif
#endif

#ifdef PLUMOS_ENABLE_MALI_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#ifdef PLUMOS_ENABLE_MALI_PNG
#include <png.h>
#endif

typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef void *EGLNativeDisplayType;
typedef void *EGLNativeWindowType;

#define EGL_FALSE 0
#define EGL_TRUE 1
#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_DONT_CARE ((EGLint)-1)
#define EGL_NONE 0x3038
#define EGL_WINDOW_BIT 0x0004
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_SURFACE_TYPE 0x3033
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_DEPTH_SIZE 0x3025
#define EGL_STENCIL_SIZE 0x3026
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_OPENGL_ES_API 0x30a0
#define EGL_CONTEXT_CLIENT_VERSION 0x3098

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef char GLchar;

#define GL_FALSE 0
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_BLEND 0x0BE2
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_TRIANGLE_STRIP 0x0005
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_RGBA 0x1908
#define GL_LINEAR 0x2601
#define GL_TEXTURE0 0x84C0
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_VERTEX_SHADER 0x8b31
#define GL_FRAGMENT_SHADER 0x8b30
#define GL_COMPILE_STATUS 0x8b81
#define GL_LINK_STATUS 0x8b82

enum plumos_mali_rotation {
  PLUMOS_MALI_ROTATION_NONE = 0,
  PLUMOS_MALI_ROTATION_CW = 1,
  PLUMOS_MALI_ROTATION_CCW = 2
};

#ifdef PLUMOS_ENABLE_MALI_FREETYPE
struct plumos_mali_ft_advance_cache_entry {
  unsigned int codepoint;
  int scale;
  int advance;
  int valid;
};
#endif

struct plumos_mali_egl_api {
  void *handle;
  EGLDisplay (*GetDisplay)(EGLNativeDisplayType display_id);
  EGLBoolean (*Initialize)(EGLDisplay display, EGLint *major, EGLint *minor);
  EGLBoolean (*ChooseConfig)(EGLDisplay display, const EGLint *attribs, EGLConfig *configs,
                             EGLint config_size, EGLint *num_config);
  EGLBoolean (*BindAPI)(EGLenum api);
  EGLSurface (*CreateWindowSurface)(EGLDisplay display, EGLConfig config,
                                    EGLNativeWindowType native_window,
                                    const EGLint *attribs);
  EGLContext (*CreateContext)(EGLDisplay display, EGLConfig config, EGLContext share_context,
                              const EGLint *attribs);
  EGLBoolean (*MakeCurrent)(EGLDisplay display, EGLSurface draw, EGLSurface read,
                            EGLContext context);
  EGLBoolean (*SwapBuffers)(EGLDisplay display, EGLSurface surface);
  EGLBoolean (*SwapInterval)(EGLDisplay display, EGLint interval);
  EGLBoolean (*QuerySurface)(EGLDisplay display, EGLSurface surface, EGLint attribute,
                             EGLint *value);
  EGLint (*GetError)(void);
  EGLBoolean (*DestroyContext)(EGLDisplay display, EGLContext context);
  EGLBoolean (*DestroySurface)(EGLDisplay display, EGLSurface surface);
  EGLBoolean (*Terminate)(EGLDisplay display);
};

struct plumos_mali_gl_api {
  void *handle;
  const unsigned char *(*GetString)(GLenum name);
  void (*Viewport)(GLint x, GLint y, GLsizei width, GLsizei height);
  void (*ClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
  void (*Clear)(GLbitfield mask);
  GLuint (*CreateShader)(GLenum type);
  void (*ShaderSource)(GLuint shader, GLsizei count, const GLchar **string,
                       const GLint *length);
  void (*CompileShader)(GLuint shader);
  void (*GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
  void (*GetShaderInfoLog)(GLuint shader, GLsizei max_length, GLsizei *length,
                           GLchar *info_log);
  void (*DeleteShader)(GLuint shader);
  GLuint (*CreateProgram)(void);
  void (*AttachShader)(GLuint program, GLuint shader);
  void (*BindAttribLocation)(GLuint program, GLuint index, const GLchar *name);
  void (*LinkProgram)(GLuint program);
  void (*GetProgramiv)(GLuint program, GLenum pname, GLint *params);
  void (*GetProgramInfoLog)(GLuint program, GLsizei max_length, GLsizei *length,
                            GLchar *info_log);
  void (*UseProgram)(GLuint program);
  GLint (*GetUniformLocation)(GLuint program, const GLchar *name);
  void (*Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
  void (*Uniform1i)(GLint location, GLint v0);
  void (*ActiveTexture)(GLenum texture);
  void (*GenTextures)(GLsizei n, GLuint *textures);
  void (*DeleteTextures)(GLsizei n, const GLuint *textures);
  void (*BindTexture)(GLenum target, GLuint texture);
  void (*TexParameteri)(GLenum target, GLenum pname, GLint param);
  void (*TexImage2D)(GLenum target, GLint level, GLint internalformat,
                     GLsizei width, GLsizei height, GLint border, GLenum format,
                     GLenum type, const void *pixels);
  void (*Enable)(GLenum cap);
  void (*Disable)(GLenum cap);
  void (*BlendFunc)(GLenum sfactor, GLenum dfactor);
  void (*EnableVertexAttribArray)(GLuint index);
  void (*DisableVertexAttribArray)(GLuint index);
  void (*VertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized,
                              GLsizei stride, const void *pointer);
  void (*DrawArrays)(GLenum mode, GLint first, GLsizei count);
  void (*Finish)(void);
  void (*DeleteProgram)(GLuint program);
};

#ifdef PLUMOS_ENABLE_MALI_PNG
#ifndef PLUMOS_MALI_GRAPHIC_TEXTURE_CACHE_SIZE
#define PLUMOS_MALI_GRAPHIC_TEXTURE_CACHE_SIZE 24
#endif

struct plumos_mali_graphic_texture_cache_entry {
  GLuint texture;
  int width;
  int height;
  unsigned long used_at;
  char path[PATH_MAX];
};
#endif

struct plumos_mali_renderer {
  struct plumos_mali_egl_api egl;
  struct plumos_mali_gl_api gl;
  int fb_fd;
  int fb_width;
  int fb_height;
  int width;
  int height;
  enum plumos_mali_rotation rotation;
  int tty_entry_scale_x10;
  long long marquee_focus_ms;
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
  GLuint program;
  GLint color_uniform;
#ifdef PLUMOS_ENABLE_MALI_PNG
  GLuint texture_program;
  GLint texture_sampler_uniform;
  unsigned long graphic_texture_tick;
  GLuint graphic_texture;
  int graphic_texture_width;
  int graphic_texture_height;
  struct plumos_mali_graphic_texture_cache_entry
      graphic_texture_cache[PLUMOS_MALI_GRAPHIC_TEXTURE_CACHE_SIZE];
#endif
#ifdef PLUMOS_ENABLE_MALI_FREETYPE
  FT_Library ft_library;
  FT_Face ft_face;
  int ft_ready;
  int ft_pixel_size;
  struct plumos_mali_ft_advance_cache_entry
      ft_advance_cache[PLUMOS_MALI_FT_ADVANCE_CACHE_SIZE];
#endif
};

static int plumos_mali_copy_string(char *out, size_t out_size, const char *in) {
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
    return 0;
  }
  memcpy(out, in, len + 1);
  return 1;
}

static enum plumos_mali_rotation plumos_mali_parse_rotation(const char *mode,
                                                            int fb_width, int fb_height) {
  if (!mode || !mode[0] || strcmp(mode, "auto") == 0) {
    return fb_height > fb_width ? PLUMOS_MALI_ROTATION_CCW : PLUMOS_MALI_ROTATION_NONE;
  }
  if (strcmp(mode, "none") == 0) {
    return PLUMOS_MALI_ROTATION_NONE;
  }
  if (strcmp(mode, "cw") == 0) {
    return PLUMOS_MALI_ROTATION_CW;
  }
  if (strcmp(mode, "ccw") == 0) {
    return PLUMOS_MALI_ROTATION_CCW;
  }
  return fb_height > fb_width ? PLUMOS_MALI_ROTATION_CCW : PLUMOS_MALI_ROTATION_NONE;
}

static int plumos_mali_parse_tty_entry_scale(const char *scale) {
  if (!scale || !scale[0] || strcmp(scale, "default") == 0 ||
      strcmp(scale, "1") == 0 || strcmp(scale, "1.0") == 0) {
    return 10;
  }
  if (strcmp(scale, "1.5") == 0 || strcmp(scale, "15") == 0) {
    return 15;
  }
  if (strcmp(scale, "2") == 0 || strcmp(scale, "2.0") == 0 ||
      strcmp(scale, "20") == 0) {
    return 20;
  }
  return 10;
}

static void plumos_mali_renderer_set_tty_entry_scale(struct plumos_mali_renderer *renderer,
                                                     const char *scale) {
  if (!renderer) {
    return;
  }
  renderer->tty_entry_scale_x10 = plumos_mali_parse_tty_entry_scale(scale);
}

static void plumos_mali_renderer_reset_marquee(struct plumos_mali_renderer *renderer) {
  struct timeval tv;
  if (!renderer) {
    return;
  }
  if (gettimeofday(&tv, NULL) != 0) {
    renderer->marquee_focus_ms = (long long)time(NULL) * 1000LL;
    return;
  }
  renderer->marquee_focus_ms = (long long)tv.tv_sec * 1000LL + (long long)(tv.tv_usec / 1000);
}

static int plumos_mali_load_symbol(void *handle, const char *name, void *fn_out,
                                   char *error, size_t error_size) {
  void *symbol;
  const char *dl_error;

  dlerror();
  symbol = dlsym(handle, name);
  if (!symbol) {
    dl_error = dlerror();
    if (error && error_size > 0) {
      snprintf(error, error_size, "dlsym failed: %s: %s", name, dl_error ? dl_error : "-");
    }
    return 0;
  }
  memcpy(fn_out, &symbol, sizeof(symbol));
  return 1;
}

#define PLUMOS_LOAD_EGL(renderer, field, symbol, error, error_size)                                \
  do {                                                                                             \
    if (!plumos_mali_load_symbol((renderer)->egl.handle, (symbol), &(renderer)->egl.field,          \
                                 (error), (error_size))) {                                         \
      return 0;                                                                                    \
    }                                                                                              \
  } while (0)

#define PLUMOS_LOAD_GL(renderer, field, symbol, error, error_size)                                 \
  do {                                                                                             \
    if (!plumos_mali_load_symbol((renderer)->gl.handle, (symbol), &(renderer)->gl.field,            \
                                 (error), (error_size))) {                                         \
      return 0;                                                                                    \
    }                                                                                              \
  } while (0)

static int plumos_mali_load_egl(struct plumos_mali_renderer *renderer, const char *path,
                                char *error, size_t error_size) {
  const char *dl_error;

  renderer->egl.handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
  if (!renderer->egl.handle) {
    dl_error = dlerror();
    if (error && error_size > 0) {
      snprintf(error, error_size, "dlopen EGL failed: %s: %s", path, dl_error ? dl_error : "-");
    }
    return 0;
  }

  PLUMOS_LOAD_EGL(renderer, GetDisplay, "eglGetDisplay", error, error_size);
  PLUMOS_LOAD_EGL(renderer, Initialize, "eglInitialize", error, error_size);
  PLUMOS_LOAD_EGL(renderer, ChooseConfig, "eglChooseConfig", error, error_size);
  PLUMOS_LOAD_EGL(renderer, BindAPI, "eglBindAPI", error, error_size);
  PLUMOS_LOAD_EGL(renderer, CreateWindowSurface, "eglCreateWindowSurface", error, error_size);
  PLUMOS_LOAD_EGL(renderer, CreateContext, "eglCreateContext", error, error_size);
  PLUMOS_LOAD_EGL(renderer, MakeCurrent, "eglMakeCurrent", error, error_size);
  PLUMOS_LOAD_EGL(renderer, SwapBuffers, "eglSwapBuffers", error, error_size);
  PLUMOS_LOAD_EGL(renderer, SwapInterval, "eglSwapInterval", error, error_size);
  PLUMOS_LOAD_EGL(renderer, QuerySurface, "eglQuerySurface", error, error_size);
  PLUMOS_LOAD_EGL(renderer, GetError, "eglGetError", error, error_size);
  PLUMOS_LOAD_EGL(renderer, DestroyContext, "eglDestroyContext", error, error_size);
  PLUMOS_LOAD_EGL(renderer, DestroySurface, "eglDestroySurface", error, error_size);
  PLUMOS_LOAD_EGL(renderer, Terminate, "eglTerminate", error, error_size);
  return 1;
}

static int plumos_mali_load_gl(struct plumos_mali_renderer *renderer, const char *path,
                               char *error, size_t error_size) {
  const char *dl_error;

  renderer->gl.handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
  if (!renderer->gl.handle) {
    dl_error = dlerror();
    if (error && error_size > 0) {
      snprintf(error, error_size, "dlopen GLES failed: %s: %s", path, dl_error ? dl_error : "-");
    }
    return 0;
  }

  PLUMOS_LOAD_GL(renderer, GetString, "glGetString", error, error_size);
  PLUMOS_LOAD_GL(renderer, Viewport, "glViewport", error, error_size);
  PLUMOS_LOAD_GL(renderer, ClearColor, "glClearColor", error, error_size);
  PLUMOS_LOAD_GL(renderer, Clear, "glClear", error, error_size);
  PLUMOS_LOAD_GL(renderer, CreateShader, "glCreateShader", error, error_size);
  PLUMOS_LOAD_GL(renderer, ShaderSource, "glShaderSource", error, error_size);
  PLUMOS_LOAD_GL(renderer, CompileShader, "glCompileShader", error, error_size);
  PLUMOS_LOAD_GL(renderer, GetShaderiv, "glGetShaderiv", error, error_size);
  PLUMOS_LOAD_GL(renderer, GetShaderInfoLog, "glGetShaderInfoLog", error, error_size);
  PLUMOS_LOAD_GL(renderer, DeleteShader, "glDeleteShader", error, error_size);
  PLUMOS_LOAD_GL(renderer, CreateProgram, "glCreateProgram", error, error_size);
  PLUMOS_LOAD_GL(renderer, AttachShader, "glAttachShader", error, error_size);
  PLUMOS_LOAD_GL(renderer, BindAttribLocation, "glBindAttribLocation", error, error_size);
  PLUMOS_LOAD_GL(renderer, LinkProgram, "glLinkProgram", error, error_size);
  PLUMOS_LOAD_GL(renderer, GetProgramiv, "glGetProgramiv", error, error_size);
  PLUMOS_LOAD_GL(renderer, GetProgramInfoLog, "glGetProgramInfoLog", error, error_size);
  PLUMOS_LOAD_GL(renderer, UseProgram, "glUseProgram", error, error_size);
  PLUMOS_LOAD_GL(renderer, GetUniformLocation, "glGetUniformLocation", error, error_size);
  PLUMOS_LOAD_GL(renderer, Uniform4f, "glUniform4f", error, error_size);
  PLUMOS_LOAD_GL(renderer, Uniform1i, "glUniform1i", error, error_size);
  PLUMOS_LOAD_GL(renderer, ActiveTexture, "glActiveTexture", error, error_size);
  PLUMOS_LOAD_GL(renderer, GenTextures, "glGenTextures", error, error_size);
  PLUMOS_LOAD_GL(renderer, DeleteTextures, "glDeleteTextures", error, error_size);
  PLUMOS_LOAD_GL(renderer, BindTexture, "glBindTexture", error, error_size);
  PLUMOS_LOAD_GL(renderer, TexParameteri, "glTexParameteri", error, error_size);
  PLUMOS_LOAD_GL(renderer, TexImage2D, "glTexImage2D", error, error_size);
  PLUMOS_LOAD_GL(renderer, Enable, "glEnable", error, error_size);
  PLUMOS_LOAD_GL(renderer, Disable, "glDisable", error, error_size);
  PLUMOS_LOAD_GL(renderer, BlendFunc, "glBlendFunc", error, error_size);
  PLUMOS_LOAD_GL(renderer, EnableVertexAttribArray, "glEnableVertexAttribArray", error,
                 error_size);
  PLUMOS_LOAD_GL(renderer, DisableVertexAttribArray, "glDisableVertexAttribArray", error,
                 error_size);
  PLUMOS_LOAD_GL(renderer, VertexAttribPointer, "glVertexAttribPointer", error, error_size);
  PLUMOS_LOAD_GL(renderer, DrawArrays, "glDrawArrays", error, error_size);
  PLUMOS_LOAD_GL(renderer, Finish, "glFinish", error, error_size);
  PLUMOS_LOAD_GL(renderer, DeleteProgram, "glDeleteProgram", error, error_size);
  return 1;
}

static int plumos_mali_open_fb(struct plumos_mali_renderer *renderer, const char *path,
                               char *error, size_t error_size) {
  struct fb_var_screeninfo var;

  renderer->fb_fd = open(path, O_RDWR | O_CLOEXEC);
  if (renderer->fb_fd < 0) {
    if (error && error_size > 0) {
      snprintf(error, error_size, "open framebuffer failed: %s: %s", path, strerror(errno));
    }
    return 0;
  }
  renderer->width = 480;
  renderer->height = 640;
  memset(&var, 0, sizeof(var));
  if (ioctl(renderer->fb_fd, FBIOGET_VSCREENINFO, &var) == 0 && var.xres > 0 && var.yres > 0) {
    renderer->width = (int)var.xres;
    renderer->height = (int)var.yres;
  }
  return 1;
}

static int plumos_mali_choose_config(struct plumos_mali_renderer *renderer, EGLConfig *config,
                                     char *error, size_t error_size) {
  EGLint attribs[] = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, EGL_DONT_CARE,
      EGL_STENCIL_SIZE, EGL_DONT_CARE,
      EGL_NONE
  };
  EGLint count = 0;

  if (renderer->egl.ChooseConfig(renderer->display, attribs, config, 1, &count) != EGL_TRUE ||
      count <= 0) {
    if (error && error_size > 0) {
      snprintf(error, error_size, "eglChooseConfig failed: 0x%04x",
               renderer->egl.GetError());
    }
    return 0;
  }
  return 1;
}

static GLuint plumos_mali_compile_shader(struct plumos_mali_renderer *renderer, GLenum type,
                                         const char *source, char *error, size_t error_size) {
  GLuint shader;
  GLint ok = 0;
  GLsizei log_len = 0;
  char log[256];

  shader = renderer->gl.CreateShader(type);
  if (!shader) {
    plumos_mali_copy_string(error, error_size, "glCreateShader failed");
    return 0;
  }
  renderer->gl.ShaderSource(shader, 1, &source, NULL);
  renderer->gl.CompileShader(shader);
  renderer->gl.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    log[0] = '\0';
    renderer->gl.GetShaderInfoLog(shader, sizeof(log), &log_len, log);
    if (error && error_size > 0) {
      snprintf(error, error_size, "shader compile failed: %.200s", log[0] ? log : "-");
    }
    renderer->gl.DeleteShader(shader);
    return 0;
  }
  return shader;
}

static int plumos_mali_setup_program(struct plumos_mali_renderer *renderer,
                                     char *error, size_t error_size) {
  static const char *vertex_source =
      "attribute vec2 a_pos;\n"
      "void main() {\n"
      "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
      "}\n";
  static const char *fragment_source =
      "precision mediump float;\n"
      "uniform vec4 u_color;\n"
      "void main() {\n"
      "  gl_FragColor = u_color;\n"
      "}\n";
  GLuint vertex_shader;
  GLuint fragment_shader;
  GLint ok = 0;
  GLsizei log_len = 0;
  char log[256];

  vertex_shader = plumos_mali_compile_shader(renderer, GL_VERTEX_SHADER, vertex_source,
                                             error, error_size);
  if (!vertex_shader) {
    return 0;
  }
  fragment_shader = plumos_mali_compile_shader(renderer, GL_FRAGMENT_SHADER, fragment_source,
                                               error, error_size);
  if (!fragment_shader) {
    renderer->gl.DeleteShader(vertex_shader);
    return 0;
  }

  renderer->program = renderer->gl.CreateProgram();
  renderer->gl.AttachShader(renderer->program, vertex_shader);
  renderer->gl.AttachShader(renderer->program, fragment_shader);
  renderer->gl.BindAttribLocation(renderer->program, 0, "a_pos");
  renderer->gl.LinkProgram(renderer->program);
  renderer->gl.GetProgramiv(renderer->program, GL_LINK_STATUS, &ok);
  renderer->gl.DeleteShader(vertex_shader);
  renderer->gl.DeleteShader(fragment_shader);
  if (!ok) {
    log[0] = '\0';
    renderer->gl.GetProgramInfoLog(renderer->program, sizeof(log), &log_len, log);
    if (error && error_size > 0) {
      snprintf(error, error_size, "program link failed: %.200s", log[0] ? log : "-");
    }
    return 0;
  }
  renderer->color_uniform = renderer->gl.GetUniformLocation(renderer->program, "u_color");
  renderer->gl.UseProgram(renderer->program);
  renderer->gl.EnableVertexAttribArray(0);
  return 1;
}

#ifdef PLUMOS_ENABLE_MALI_PNG
static int plumos_mali_setup_texture_program(struct plumos_mali_renderer *renderer,
                                             char *error, size_t error_size) {
  static const char *vertex_source =
      "attribute vec2 a_pos;\n"
      "attribute vec2 a_tex;\n"
      "varying vec2 v_tex;\n"
      "void main() {\n"
      "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
      "  v_tex = a_tex;\n"
      "}\n";
  static const char *fragment_source =
      "precision mediump float;\n"
      "varying vec2 v_tex;\n"
      "uniform sampler2D u_tex;\n"
      "void main() {\n"
      "  gl_FragColor = texture2D(u_tex, v_tex);\n"
      "}\n";
  GLuint vertex_shader;
  GLuint fragment_shader;
  GLint ok = 0;
  GLsizei log_len = 0;
  char log[256];

  vertex_shader = plumos_mali_compile_shader(renderer, GL_VERTEX_SHADER, vertex_source,
                                             error, error_size);
  if (!vertex_shader) {
    return 0;
  }
  fragment_shader = plumos_mali_compile_shader(renderer, GL_FRAGMENT_SHADER, fragment_source,
                                               error, error_size);
  if (!fragment_shader) {
    renderer->gl.DeleteShader(vertex_shader);
    return 0;
  }

  renderer->texture_program = renderer->gl.CreateProgram();
  renderer->gl.AttachShader(renderer->texture_program, vertex_shader);
  renderer->gl.AttachShader(renderer->texture_program, fragment_shader);
  renderer->gl.BindAttribLocation(renderer->texture_program, 0, "a_pos");
  renderer->gl.BindAttribLocation(renderer->texture_program, 1, "a_tex");
  renderer->gl.LinkProgram(renderer->texture_program);
  renderer->gl.GetProgramiv(renderer->texture_program, GL_LINK_STATUS, &ok);
  renderer->gl.DeleteShader(vertex_shader);
  renderer->gl.DeleteShader(fragment_shader);
  if (!ok) {
    log[0] = '\0';
    renderer->gl.GetProgramInfoLog(renderer->texture_program, sizeof(log), &log_len, log);
    if (error && error_size > 0) {
      snprintf(error, error_size, "texture program link failed: %.200s",
               log[0] ? log : "-");
    }
    return 0;
  }
  renderer->texture_sampler_uniform =
      renderer->gl.GetUniformLocation(renderer->texture_program, "u_tex");
  renderer->gl.UseProgram(renderer->program);
  return 1;
}
#endif

static int plumos_mali_renderer_init(struct plumos_mali_renderer *renderer, const char *fb_path,
                                     const char *egl_path, const char *gles_path,
                                     const char *rotation_mode, char *error,
                                     size_t error_size) {
  EGLConfig config = NULL;
  EGLint major = 0;
  EGLint minor = 0;
  EGLint width = 0;
  EGLint height = 0;
  EGLint surface_attribs[] = { EGL_NONE };
  EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

  memset(renderer, 0, sizeof(*renderer));
  renderer->fb_fd = -1;
  renderer->display = EGL_NO_DISPLAY;
  renderer->surface = EGL_NO_SURFACE;
  renderer->context = EGL_NO_CONTEXT;
  renderer->fb_width = 480;
  renderer->fb_height = 640;
  renderer->width = 480;
  renderer->height = 640;
  if (!fb_path || !fb_path[0]) {
    fb_path = "/dev/fb0";
  }
  if (!egl_path || !egl_path[0]) {
    egl_path = "/usr/lib/libEGL.so";
  }
  if (!gles_path || !gles_path[0]) {
    gles_path = "/usr/lib/libGLESv2.so";
  }

  if (!plumos_mali_open_fb(renderer, fb_path, error, error_size) ||
      !plumos_mali_load_egl(renderer, egl_path, error, error_size) ||
      !plumos_mali_load_gl(renderer, gles_path, error, error_size)) {
    return 0;
  }

  renderer->display = renderer->egl.GetDisplay(EGL_DEFAULT_DISPLAY);
  if (renderer->display == EGL_NO_DISPLAY) {
    if (error && error_size > 0) {
      snprintf(error, error_size, "eglGetDisplay failed: 0x%04x", renderer->egl.GetError());
    }
    return 0;
  }
  if (renderer->egl.Initialize(renderer->display, &major, &minor) != EGL_TRUE) {
    if (error && error_size > 0) {
      snprintf(error, error_size, "eglInitialize failed: 0x%04x", renderer->egl.GetError());
    }
    return 0;
  }
  if (renderer->egl.BindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
    if (error && error_size > 0) {
      snprintf(error, error_size, "eglBindAPI failed: 0x%04x", renderer->egl.GetError());
    }
    return 0;
  }
  if (!plumos_mali_choose_config(renderer, &config, error, error_size)) {
    return 0;
  }

  renderer->surface =
      renderer->egl.CreateWindowSurface(renderer->display, config, NULL, surface_attribs);
  if (renderer->surface == EGL_NO_SURFACE) {
    if (error && error_size > 0) {
      snprintf(error, error_size, "eglCreateWindowSurface(NULL) failed: 0x%04x",
               renderer->egl.GetError());
    }
    return 0;
  }
  renderer->context =
      renderer->egl.CreateContext(renderer->display, config, EGL_NO_CONTEXT, context_attribs);
  if (renderer->context == EGL_NO_CONTEXT) {
    if (error && error_size > 0) {
      snprintf(error, error_size, "eglCreateContext failed: 0x%04x", renderer->egl.GetError());
    }
    return 0;
  }
  if (renderer->egl.MakeCurrent(renderer->display, renderer->surface, renderer->surface,
                                renderer->context) != EGL_TRUE) {
    if (error && error_size > 0) {
      snprintf(error, error_size, "eglMakeCurrent failed: 0x%04x", renderer->egl.GetError());
    }
    return 0;
  }
  renderer->egl.SwapInterval(renderer->display, 0);
  if (renderer->egl.QuerySurface(renderer->display, renderer->surface, EGL_WIDTH, &width) ==
          EGL_TRUE &&
      renderer->egl.QuerySurface(renderer->display, renderer->surface, EGL_HEIGHT, &height) ==
          EGL_TRUE &&
      width > 0 && height > 0) {
    renderer->fb_width = width;
    renderer->fb_height = height;
  }
  renderer->rotation =
      plumos_mali_parse_rotation(rotation_mode, renderer->fb_width, renderer->fb_height);
  if (renderer->rotation == PLUMOS_MALI_ROTATION_CW ||
      renderer->rotation == PLUMOS_MALI_ROTATION_CCW) {
    renderer->width = renderer->fb_height;
    renderer->height = renderer->fb_width;
  } else {
    renderer->width = renderer->fb_width;
    renderer->height = renderer->fb_height;
  }
  renderer->gl.Viewport(0, 0, renderer->fb_width, renderer->fb_height);
  if (!plumos_mali_setup_program(renderer, error, error_size)) {
    return 0;
  }
#ifdef PLUMOS_ENABLE_MALI_PNG
  if (!plumos_mali_setup_texture_program(renderer, error, error_size)) {
    return 0;
  }
#endif
  return 1;
}

#ifdef PLUMOS_ENABLE_MALI_FREETYPE
static int plumos_mali_renderer_load_font(struct plumos_mali_renderer *renderer,
                                          const char *font_path, char *error,
                                          size_t error_size) {
  FT_Error ft_error;

  if (!renderer || !font_path || !font_path[0]) {
    if (error && error_size > 0) {
      snprintf(error, error_size, "font path is empty");
    }
    return 0;
  }
  if (!renderer->ft_library) {
    ft_error = FT_Init_FreeType(&renderer->ft_library);
    if (ft_error) {
      if (error && error_size > 0) {
        snprintf(error, error_size, "FT_Init_FreeType failed: %d", (int)ft_error);
      }
      return 0;
    }
  }
  if (renderer->ft_face) {
    FT_Done_Face(renderer->ft_face);
    renderer->ft_face = NULL;
    renderer->ft_ready = 0;
  }
  ft_error = FT_New_Face(renderer->ft_library, font_path, 0, &renderer->ft_face);
  if (ft_error) {
    if (error && error_size > 0) {
      snprintf(error, error_size, "FT_New_Face failed: %d", (int)ft_error);
    }
    return 0;
  }
  renderer->ft_ready = 1;
  renderer->ft_pixel_size = 0;
  memset(renderer->ft_advance_cache, 0, sizeof(renderer->ft_advance_cache));
  return 1;
}
#endif

static void plumos_mali_renderer_shutdown(struct plumos_mali_renderer *renderer) {
  if (!renderer) {
    return;
  }
#ifdef PLUMOS_ENABLE_MALI_FREETYPE
  if (renderer->ft_face) {
    FT_Done_Face(renderer->ft_face);
    renderer->ft_face = NULL;
  }
  if (renderer->ft_library) {
    FT_Done_FreeType(renderer->ft_library);
    renderer->ft_library = NULL;
  }
#endif
  if (renderer->display != EGL_NO_DISPLAY) {
#ifdef PLUMOS_ENABLE_MALI_PNG
    if (renderer->context != EGL_NO_CONTEXT && renderer->gl.handle) {
      size_t i;
      for (i = 0; i < PLUMOS_MALI_GRAPHIC_TEXTURE_CACHE_SIZE; i++) {
        if (renderer->graphic_texture_cache[i].texture) {
          renderer->gl.DeleteTextures(1, &renderer->graphic_texture_cache[i].texture);
          renderer->graphic_texture_cache[i].texture = 0;
        }
      }
      renderer->graphic_texture = 0;
    }
    if (renderer->context != EGL_NO_CONTEXT && renderer->gl.handle &&
        renderer->texture_program) {
      renderer->gl.DeleteProgram(renderer->texture_program);
      renderer->texture_program = 0;
    }
#endif
    if (renderer->context != EGL_NO_CONTEXT && renderer->gl.handle && renderer->program) {
      renderer->gl.DeleteProgram(renderer->program);
    }
    if (renderer->context != EGL_NO_CONTEXT) {
      renderer->egl.MakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                                EGL_NO_CONTEXT);
      renderer->egl.DestroyContext(renderer->display, renderer->context);
    }
    if (renderer->surface != EGL_NO_SURFACE) {
      renderer->egl.DestroySurface(renderer->display, renderer->surface);
    }
    renderer->egl.Terminate(renderer->display);
  }
  if (renderer->fb_fd >= 0) {
    close(renderer->fb_fd);
  }
  if (renderer->gl.handle) {
    dlclose(renderer->gl.handle);
  }
  if (renderer->egl.handle) {
    dlclose(renderer->egl.handle);
  }
  memset(renderer, 0, sizeof(*renderer));
  renderer->fb_fd = -1;
}

static const unsigned char *plumos_mali_glyph(char ch) {
  static const unsigned char blank[7] = {0, 0, 0, 0, 0, 0, 0};
  static const unsigned char unknown[7] = {14, 17, 1, 2, 4, 0, 4};
  static const unsigned char glyph_0[7] = {14, 17, 19, 21, 25, 17, 14};
  static const unsigned char glyph_1[7] = {4, 12, 4, 4, 4, 4, 14};
  static const unsigned char glyph_2[7] = {14, 17, 1, 2, 4, 8, 31};
  static const unsigned char glyph_3[7] = {30, 1, 1, 14, 1, 1, 30};
  static const unsigned char glyph_4[7] = {2, 6, 10, 18, 31, 2, 2};
  static const unsigned char glyph_5[7] = {31, 16, 30, 1, 1, 17, 14};
  static const unsigned char glyph_6[7] = {6, 8, 16, 30, 17, 17, 14};
  static const unsigned char glyph_7[7] = {31, 1, 2, 4, 8, 8, 8};
  static const unsigned char glyph_8[7] = {14, 17, 17, 14, 17, 17, 14};
  static const unsigned char glyph_9[7] = {14, 17, 17, 15, 1, 2, 12};
  static const unsigned char glyph_a[7] = {14, 17, 17, 31, 17, 17, 17};
  static const unsigned char glyph_b[7] = {30, 17, 17, 30, 17, 17, 30};
  static const unsigned char glyph_c[7] = {14, 17, 16, 16, 16, 17, 14};
  static const unsigned char glyph_d[7] = {30, 17, 17, 17, 17, 17, 30};
  static const unsigned char glyph_e[7] = {31, 16, 16, 30, 16, 16, 31};
  static const unsigned char glyph_f[7] = {31, 16, 16, 30, 16, 16, 16};
  static const unsigned char glyph_g[7] = {14, 17, 16, 23, 17, 17, 14};
  static const unsigned char glyph_h[7] = {17, 17, 17, 31, 17, 17, 17};
  static const unsigned char glyph_i[7] = {14, 4, 4, 4, 4, 4, 14};
  static const unsigned char glyph_j[7] = {7, 2, 2, 2, 18, 18, 12};
  static const unsigned char glyph_k[7] = {17, 18, 20, 24, 20, 18, 17};
  static const unsigned char glyph_l[7] = {16, 16, 16, 16, 16, 16, 31};
  static const unsigned char glyph_m[7] = {17, 27, 21, 21, 17, 17, 17};
  static const unsigned char glyph_n[7] = {17, 25, 21, 19, 17, 17, 17};
  static const unsigned char glyph_o[7] = {14, 17, 17, 17, 17, 17, 14};
  static const unsigned char glyph_p[7] = {30, 17, 17, 30, 16, 16, 16};
  static const unsigned char glyph_q[7] = {14, 17, 17, 17, 21, 18, 13};
  static const unsigned char glyph_r[7] = {30, 17, 17, 30, 20, 18, 17};
  static const unsigned char glyph_s[7] = {15, 16, 16, 14, 1, 1, 30};
  static const unsigned char glyph_t[7] = {31, 4, 4, 4, 4, 4, 4};
  static const unsigned char glyph_u[7] = {17, 17, 17, 17, 17, 17, 14};
  static const unsigned char glyph_v[7] = {17, 17, 17, 17, 17, 10, 4};
  static const unsigned char glyph_w[7] = {17, 17, 17, 21, 21, 21, 10};
  static const unsigned char glyph_x[7] = {17, 17, 10, 4, 10, 17, 17};
  static const unsigned char glyph_y[7] = {17, 17, 10, 4, 4, 4, 4};
  static const unsigned char glyph_z[7] = {31, 1, 2, 4, 8, 16, 31};
  static const unsigned char glyph_lower_a[7] = {0, 0, 14, 1, 15, 17, 15};
  static const unsigned char glyph_lower_b[7] = {16, 16, 22, 25, 17, 17, 30};
  static const unsigned char glyph_lower_c[7] = {0, 0, 14, 16, 16, 17, 14};
  static const unsigned char glyph_lower_d[7] = {1, 1, 13, 19, 17, 17, 15};
  static const unsigned char glyph_lower_e[7] = {0, 0, 14, 17, 31, 16, 14};
  static const unsigned char glyph_lower_f[7] = {6, 8, 8, 28, 8, 8, 8};
  static const unsigned char glyph_lower_g[7] = {0, 0, 15, 17, 15, 1, 14};
  static const unsigned char glyph_lower_h[7] = {16, 16, 22, 25, 17, 17, 17};
  static const unsigned char glyph_lower_i[7] = {4, 0, 12, 4, 4, 4, 14};
  static const unsigned char glyph_lower_j[7] = {2, 0, 6, 2, 2, 18, 12};
  static const unsigned char glyph_lower_k[7] = {16, 16, 18, 20, 24, 20, 18};
  static const unsigned char glyph_lower_l[7] = {12, 4, 4, 4, 4, 4, 14};
  static const unsigned char glyph_lower_m[7] = {0, 0, 26, 21, 21, 21, 21};
  static const unsigned char glyph_lower_n[7] = {0, 0, 22, 25, 17, 17, 17};
  static const unsigned char glyph_lower_o[7] = {0, 0, 14, 17, 17, 17, 14};
  static const unsigned char glyph_lower_p[7] = {0, 0, 30, 17, 30, 16, 16};
  static const unsigned char glyph_lower_q[7] = {0, 0, 15, 17, 15, 1, 1};
  static const unsigned char glyph_lower_r[7] = {0, 0, 22, 25, 16, 16, 16};
  static const unsigned char glyph_lower_s[7] = {0, 0, 15, 16, 14, 1, 30};
  static const unsigned char glyph_lower_t[7] = {8, 8, 28, 8, 8, 9, 6};
  static const unsigned char glyph_lower_u[7] = {0, 0, 17, 17, 17, 19, 13};
  static const unsigned char glyph_lower_v[7] = {0, 0, 17, 17, 17, 10, 4};
  static const unsigned char glyph_lower_w[7] = {0, 0, 17, 17, 21, 21, 10};
  static const unsigned char glyph_lower_x[7] = {0, 0, 17, 10, 4, 10, 17};
  static const unsigned char glyph_lower_y[7] = {0, 0, 17, 17, 15, 1, 14};
  static const unsigned char glyph_lower_z[7] = {0, 0, 31, 2, 4, 8, 31};
  static const unsigned char dash[7] = {0, 0, 0, 31, 0, 0, 0};
  static const unsigned char underscore[7] = {0, 0, 0, 0, 0, 0, 31};
  static const unsigned char colon[7] = {0, 4, 4, 0, 4, 4, 0};
  static const unsigned char dot[7] = {0, 0, 0, 0, 0, 12, 12};
  static const unsigned char slash[7] = {1, 1, 2, 4, 8, 16, 16};
  static const unsigned char backslash[7] = {16, 16, 8, 4, 2, 1, 1};
  static const unsigned char gt[7] = {16, 8, 4, 2, 4, 8, 16};
  static const unsigned char lt[7] = {1, 2, 4, 8, 4, 2, 1};
  static const unsigned char eq[7] = {0, 0, 31, 0, 31, 0, 0};
  static const unsigned char plus[7] = {0, 4, 4, 31, 4, 4, 0};
  static const unsigned char lparen[7] = {2, 4, 8, 8, 8, 4, 2};
  static const unsigned char rparen[7] = {8, 4, 2, 2, 2, 4, 8};
  static const unsigned char lbracket[7] = {14, 8, 8, 8, 8, 8, 14};
  static const unsigned char rbracket[7] = {14, 2, 2, 2, 2, 2, 14};
  static const unsigned char comma[7] = {0, 0, 0, 0, 0, 4, 8};
  static const unsigned char bang[7] = {4, 4, 4, 4, 4, 0, 4};
  static const unsigned char pipe_glyph[7] = {4, 4, 4, 4, 4, 4, 4};
  static const unsigned char quote[7] = {10, 10, 0, 0, 0, 0, 0};
  static const unsigned char star[7] = {0, 21, 14, 31, 14, 21, 0};
  static const unsigned char tilde[7] = {0, 0, 8, 21, 2, 0, 0};
  static const unsigned char at[7] = {14, 17, 23, 21, 23, 16, 14};
  static const unsigned char hash[7] = {10, 31, 10, 10, 31, 10, 10};
  static const unsigned char dollar[7] = {4, 15, 20, 14, 5, 30, 4};
  static const unsigned char percent[7] = {17, 18, 2, 4, 8, 9, 17};
  static const unsigned char amp[7] = {12, 18, 20, 8, 21, 18, 13};
  static const unsigned char semicolon[7] = {0, 4, 4, 0, 4, 4, 8};
  static const unsigned char grave[7] = {8, 4, 0, 0, 0, 0, 0};
  static const unsigned char caret[7] = {4, 10, 17, 0, 0, 0, 0};
  static const unsigned char lbrace[7] = {2, 4, 4, 8, 4, 4, 2};
  static const unsigned char rbrace[7] = {8, 4, 4, 2, 4, 4, 8};

  switch (ch) {
  case ' ':
    return blank;
  case '0':
    return glyph_0;
  case '1':
    return glyph_1;
  case '2':
    return glyph_2;
  case '3':
    return glyph_3;
  case '4':
    return glyph_4;
  case '5':
    return glyph_5;
  case '6':
    return glyph_6;
  case '7':
    return glyph_7;
  case '8':
    return glyph_8;
  case '9':
    return glyph_9;
  case 'A':
    return glyph_a;
  case 'B':
    return glyph_b;
  case 'C':
    return glyph_c;
  case 'D':
    return glyph_d;
  case 'E':
    return glyph_e;
  case 'F':
    return glyph_f;
  case 'G':
    return glyph_g;
  case 'H':
    return glyph_h;
  case 'I':
    return glyph_i;
  case 'J':
    return glyph_j;
  case 'K':
    return glyph_k;
  case 'L':
    return glyph_l;
  case 'M':
    return glyph_m;
  case 'N':
    return glyph_n;
  case 'O':
    return glyph_o;
  case 'P':
    return glyph_p;
  case 'Q':
    return glyph_q;
  case 'R':
    return glyph_r;
  case 'S':
    return glyph_s;
  case 'T':
    return glyph_t;
  case 'U':
    return glyph_u;
  case 'V':
    return glyph_v;
  case 'W':
    return glyph_w;
  case 'X':
    return glyph_x;
  case 'Y':
    return glyph_y;
  case 'Z':
    return glyph_z;
  case 'a':
    return glyph_lower_a;
  case 'b':
    return glyph_lower_b;
  case 'c':
    return glyph_lower_c;
  case 'd':
    return glyph_lower_d;
  case 'e':
    return glyph_lower_e;
  case 'f':
    return glyph_lower_f;
  case 'g':
    return glyph_lower_g;
  case 'h':
    return glyph_lower_h;
  case 'i':
    return glyph_lower_i;
  case 'j':
    return glyph_lower_j;
  case 'k':
    return glyph_lower_k;
  case 'l':
    return glyph_lower_l;
  case 'm':
    return glyph_lower_m;
  case 'n':
    return glyph_lower_n;
  case 'o':
    return glyph_lower_o;
  case 'p':
    return glyph_lower_p;
  case 'q':
    return glyph_lower_q;
  case 'r':
    return glyph_lower_r;
  case 's':
    return glyph_lower_s;
  case 't':
    return glyph_lower_t;
  case 'u':
    return glyph_lower_u;
  case 'v':
    return glyph_lower_v;
  case 'w':
    return glyph_lower_w;
  case 'x':
    return glyph_lower_x;
  case 'y':
    return glyph_lower_y;
  case 'z':
    return glyph_lower_z;
  case '-':
    return dash;
  case '_':
    return underscore;
  case ':':
    return colon;
  case '.':
    return dot;
  case '/':
    return slash;
  case '\\':
    return backslash;
  case '>':
    return gt;
  case '<':
    return lt;
  case '=':
    return eq;
  case '+':
    return plus;
  case '(':
    return lparen;
  case ')':
    return rparen;
  case '[':
    return lbracket;
  case ']':
    return rbracket;
  case ',':
    return comma;
  case '!':
    return bang;
  case '|':
    return pipe_glyph;
  case '"':
  case '\'':
    return quote;
  case '*':
    return star;
  case '~':
    return tilde;
  case '@':
    return at;
  case '#':
    return hash;
  case '$':
    return dollar;
  case '%':
    return percent;
  case '&':
    return amp;
  case ';':
    return semicolon;
  case '`':
    return grave;
  case '^':
    return caret;
  case '{':
    return lbrace;
  case '}':
    return rbrace;
  case '?':
    return unknown;
  default:
    return unknown;
  }
}

static const char *plumos_mali_utf8_next(const char *s, unsigned int *codepoint) {
  const unsigned char *p = (const unsigned char *)s;
  unsigned int cp;

  if (!p || !*p) {
    if (codepoint) {
      *codepoint = 0;
    }
    return s;
  }
  if (p[0] < 0x80) {
    if (codepoint) {
      *codepoint = p[0];
    }
    return s + 1;
  }
  if ((p[0] & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
    cp = ((unsigned int)(p[0] & 0x1f) << 6) | (unsigned int)(p[1] & 0x3f);
    if (cp >= 0x80) {
      if (codepoint) {
        *codepoint = cp;
      }
      return s + 2;
    }
  } else if ((p[0] & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 &&
             (p[2] & 0xc0) == 0x80) {
    cp = ((unsigned int)(p[0] & 0x0f) << 12) |
         ((unsigned int)(p[1] & 0x3f) << 6) |
         (unsigned int)(p[2] & 0x3f);
    if (cp >= 0x800 && !(cp >= 0xd800 && cp <= 0xdfff)) {
      if (codepoint) {
        *codepoint = cp;
      }
      return s + 3;
    }
  } else if ((p[0] & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 &&
             (p[2] & 0xc0) == 0x80 && (p[3] & 0xc0) == 0x80) {
    cp = ((unsigned int)(p[0] & 0x07) << 18) |
         ((unsigned int)(p[1] & 0x3f) << 12) |
         ((unsigned int)(p[2] & 0x3f) << 6) |
         (unsigned int)(p[3] & 0x3f);
    if (cp >= 0x10000 && cp <= 0x10ffff) {
      if (codepoint) {
        *codepoint = cp;
      }
      return s + 4;
    }
  }
  if (codepoint) {
    *codepoint = '?';
  }
  return s + 1;
}

static int plumos_mali_utf8_append_codepoint(char *out, size_t out_size,
                                             size_t *pos, unsigned int codepoint) {
  if (!out || !pos || *pos >= out_size) {
    return 0;
  }
  if (codepoint <= 0x7f) {
    if (*pos + 1 >= out_size) {
      return 0;
    }
    out[(*pos)++] = (char)codepoint;
  } else if (codepoint <= 0x7ff) {
    if (*pos + 2 >= out_size) {
      return 0;
    }
    out[(*pos)++] = (char)(0xc0 | (codepoint >> 6));
    out[(*pos)++] = (char)(0x80 | (codepoint & 0x3f));
  } else if (codepoint <= 0xffff) {
    if (*pos + 3 >= out_size) {
      return 0;
    }
    out[(*pos)++] = (char)(0xe0 | (codepoint >> 12));
    out[(*pos)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
    out[(*pos)++] = (char)(0x80 | (codepoint & 0x3f));
  } else if (codepoint <= 0x10ffff) {
    if (*pos + 4 >= out_size) {
      return 0;
    }
    out[(*pos)++] = (char)(0xf0 | (codepoint >> 18));
    out[(*pos)++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
    out[(*pos)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
    out[(*pos)++] = (char)(0x80 | (codepoint & 0x3f));
  } else {
    return 0;
  }
  out[*pos] = '\0';
  return 1;
}

static unsigned int plumos_mali_compose_kana_mark(unsigned int base,
                                                  unsigned int mark) {
  if (mark == 0x3099) {
    switch (base) {
    case 0x3046:
      return 0x3094;
    case 0x30A6:
      return 0x30F4;
    case 0x30EF:
      return 0x30F7;
    case 0x30F0:
      return 0x30F8;
    case 0x30F1:
      return 0x30F9;
    case 0x30F2:
      return 0x30FA;
    default:
      break;
    }
    if ((base >= 0x304B && base <= 0x3069 &&
         (base == 0x304B || base == 0x304D || base == 0x304F ||
          base == 0x3051 || base == 0x3053 || base == 0x3055 ||
          base == 0x3057 || base == 0x3059 || base == 0x305B ||
          base == 0x305D || base == 0x305F || base == 0x3061 ||
          base == 0x3064 || base == 0x3066 || base == 0x3068)) ||
        (base >= 0x30AB && base <= 0x30C8 &&
         (base == 0x30AB || base == 0x30AD || base == 0x30AF ||
          base == 0x30B1 || base == 0x30B3 || base == 0x30B5 ||
          base == 0x30B7 || base == 0x30B9 || base == 0x30BB ||
          base == 0x30BD || base == 0x30BF || base == 0x30C1 ||
          base == 0x30C4 || base == 0x30C6 || base == 0x30C8))) {
      return base + 1;
    }
    if ((base >= 0x306F && base <= 0x307B &&
         (base == 0x306F || base == 0x3072 || base == 0x3075 ||
          base == 0x3078 || base == 0x307B)) ||
        (base >= 0x30CF && base <= 0x30DB &&
         (base == 0x30CF || base == 0x30D2 || base == 0x30D5 ||
          base == 0x30D8 || base == 0x30DB))) {
      return base + 1;
    }
  } else if (mark == 0x309A) {
    if ((base >= 0x306F && base <= 0x307B &&
         (base == 0x306F || base == 0x3072 || base == 0x3075 ||
          base == 0x3078 || base == 0x307B)) ||
        (base >= 0x30CF && base <= 0x30DB &&
         (base == 0x30CF || base == 0x30D2 || base == 0x30D5 ||
          base == 0x30D8 || base == 0x30DB))) {
      return base + 2;
    }
  }
  return 0;
}

static void plumos_mali_normalize_kana_marks(const char *in, char *out,
                                             size_t out_size) {
  const char *p;
  size_t n = 0;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!in) {
    return;
  }
  p = in;
  while (*p) {
    unsigned int cp;
    unsigned int next_cp;
    unsigned int composed = 0;
    const char *next = plumos_mali_utf8_next(p, &cp);
    const char *after_next = next;

    if (*next) {
      after_next = plumos_mali_utf8_next(next, &next_cp);
      composed = plumos_mali_compose_kana_mark(cp, next_cp);
    }
    if (composed) {
      if (!plumos_mali_utf8_append_codepoint(out, out_size, &n, composed)) {
        break;
      }
      p = after_next;
    } else {
      if (!plumos_mali_utf8_append_codepoint(out, out_size, &n, cp)) {
        break;
      }
      p = next;
    }
  }
}

static int plumos_mali_utf8_cell_width(unsigned int codepoint) {
  return codepoint >= 0x80 ? 2 : 1;
}

static void plumos_mali_copy_utf8_cells(const char *in, char *out, size_t out_size,
                                        size_t max_cells) {
  const char *p;
  size_t n = 0;
  size_t cells = 0;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!in) {
    return;
  }
  p = in;
  while (*p) {
    unsigned int cp;
    const char *next = plumos_mali_utf8_next(p, &cp);
    size_t len = (size_t)(next - p);
    size_t width = (size_t)plumos_mali_utf8_cell_width(cp);
    if (cells + width > max_cells || n + len + 1 > out_size) {
      break;
    }
    memcpy(out + n, p, len);
    n += len;
    cells += width;
    p = next;
  }
  out[n] = '\0';
}

static void plumos_mali_rect_to_ndc(struct plumos_mali_renderer *renderer,
                                    float x, float y, float w, float h,
                                    GLfloat *x0, GLfloat *y0,
                                    GLfloat *x1, GLfloat *y1) {
  float fx = x;
  float fy = y;
  float fw = w;
  float fh = h;

  if (renderer->rotation == PLUMOS_MALI_ROTATION_CCW) {
    fx = y;
    fy = (float)renderer->fb_height - (x + w);
    fw = h;
    fh = w;
  } else if (renderer->rotation == PLUMOS_MALI_ROTATION_CW) {
    fx = (float)renderer->fb_width - (y + h);
    fy = x;
    fw = h;
    fh = w;
  }

  *x0 = (fx / (float)renderer->fb_width) * 2.0f - 1.0f;
  *x1 = ((fx + fw) / (float)renderer->fb_width) * 2.0f - 1.0f;
  *y0 = 1.0f - (fy / (float)renderer->fb_height) * 2.0f;
  *y1 = 1.0f - ((fy + fh) / (float)renderer->fb_height) * 2.0f;
}

static void plumos_mali_point_to_ndc(struct plumos_mali_renderer *renderer,
                                     float x, float y, GLfloat *out_x,
                                     GLfloat *out_y) {
  float fx = x;
  float fy = y;

  if (renderer->rotation == PLUMOS_MALI_ROTATION_CCW) {
    fx = y;
    fy = (float)renderer->fb_height - x;
  } else if (renderer->rotation == PLUMOS_MALI_ROTATION_CW) {
    fx = (float)renderer->fb_width - y;
    fy = x;
  }
  *out_x = (fx / (float)renderer->fb_width) * 2.0f - 1.0f;
  *out_y = 1.0f - (fy / (float)renderer->fb_height) * 2.0f;
}

static void plumos_mali_rect(struct plumos_mali_renderer *renderer, float x, float y,
                             float w, float h, float red, float green, float blue,
                             float alpha) {
  GLfloat x0;
  GLfloat x1;
  GLfloat y0;
  GLfloat y1;
  GLfloat verts[8];

  plumos_mali_rect_to_ndc(renderer, x, y, w, h, &x0, &y0, &x1, &y1);
  verts[0] = x0;
  verts[1] = y0;
  verts[2] = x1;
  verts[3] = y0;
  verts[4] = x0;
  verts[5] = y1;
  verts[6] = x1;
  verts[7] = y1;

  renderer->gl.Uniform4f(renderer->color_uniform, red, green, blue, alpha);
  renderer->gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
  renderer->gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

#ifdef PLUMOS_ENABLE_MALI_PNG
static int plumos_mali_path_has_png_ext(const char *path) {
  const char *ext;

  if (!path || !path[0]) {
    return 0;
  }
  ext = strrchr(path, '.');
  return ext && strcasecmp(ext, ".png") == 0;
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclobbered"
#endif
static int plumos_mali_load_png_rgba(const char *path, unsigned char **pixels_out,
                                     int *width_out, int *height_out) {
  FILE *f;
  png_structp png = NULL;
  png_infop info = NULL;
  png_bytep *rows = NULL;
  png_bytep pixels = NULL;
  png_uint_32 width;
  png_uint_32 height;
  int bit_depth;
  int color_type;
  int interlace_type;
  size_t row_bytes;
  png_uint_32 y;
  int ok = 0;

  if (!path || !pixels_out || !width_out || !height_out ||
      !plumos_mali_path_has_png_ext(path)) {
    return 0;
  }
  *pixels_out = NULL;
  *width_out = 0;
  *height_out = 0;
  f = fopen(path, "rb");
  if (!f) {
    return 0;
  }

  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    fclose(f);
    return 0;
  }
  info = png_create_info_struct(png);
  if (!info) {
    png_destroy_read_struct(&png, NULL, NULL);
    fclose(f);
    return 0;
  }
  if (setjmp(png_jmpbuf(png))) {
    goto done;
  }

  png_init_io(png, f);
  png_read_info(png, info);
  png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type,
               &interlace_type, NULL, NULL);
  if (width == 0 || height == 0 || width > 2048 || height > 2048) {
    goto done;
  }
  if (bit_depth == 16) {
    png_set_strip_16(png);
  }
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(png);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    png_set_expand_gray_1_2_4_to_8(png);
  }
  if (png_get_valid(png, info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png);
  }
  if ((color_type & PNG_COLOR_MASK_ALPHA) == 0) {
    png_set_filler(png, 0xff, PNG_FILLER_AFTER);
  }
  (void)png_set_interlace_handling(png);
  png_read_update_info(png, info);
  row_bytes = png_get_rowbytes(png, info);
  if (row_bytes != (size_t)width * 4u) {
    goto done;
  }

  pixels = (png_bytep)malloc(row_bytes * (size_t)height);
  rows = (png_bytep *)malloc(sizeof(png_bytep) * (size_t)height);
  if (!pixels || !rows) {
    goto done;
  }
  for (y = 0; y < height; y++) {
    rows[y] = pixels + row_bytes * (size_t)y;
  }
  png_read_image(png, rows);
  png_read_end(png, NULL);
  *pixels_out = pixels;
  *width_out = (int)width;
  *height_out = (int)height;
  pixels = NULL;
  ok = 1;

done:
  free(rows);
  free(pixels);
  png_destroy_read_struct(&png, &info, NULL);
  fclose(f);
  return ok;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int plumos_mali_graphic_load_texture(struct plumos_mali_renderer *renderer,
                                            const char *path) {
  unsigned char *pixels = NULL;
  int width = 0;
  int height = 0;
  GLuint texture = 0;
  size_t i;
  size_t slot = 0;
  unsigned long oldest_used = 0;

  if (!renderer || !path || !path[0] || !plumos_mali_path_has_png_ext(path)) {
    return 0;
  }
  for (i = 0; i < PLUMOS_MALI_GRAPHIC_TEXTURE_CACHE_SIZE; i++) {
    struct plumos_mali_graphic_texture_cache_entry *entry =
        &renderer->graphic_texture_cache[i];
    if (entry->texture && strcmp(entry->path, path) == 0) {
      entry->used_at = ++renderer->graphic_texture_tick;
      renderer->graphic_texture = entry->texture;
      renderer->graphic_texture_width = entry->width;
      renderer->graphic_texture_height = entry->height;
      return 1;
    }
  }
  if (!plumos_mali_load_png_rgba(path, &pixels, &width, &height)) {
    return 0;
  }
  renderer->gl.GenTextures(1, &texture);
  if (!texture) {
    free(pixels);
    return 0;
  }
  renderer->gl.BindTexture(GL_TEXTURE_2D, texture);
  renderer->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  renderer->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  renderer->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  renderer->gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  renderer->gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                          GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  free(pixels);

  for (i = 0; i < PLUMOS_MALI_GRAPHIC_TEXTURE_CACHE_SIZE; i++) {
    struct plumos_mali_graphic_texture_cache_entry *entry =
        &renderer->graphic_texture_cache[i];
    if (!entry->texture) {
      slot = i;
      break;
    }
    if (i == 0 || entry->used_at < oldest_used) {
      oldest_used = entry->used_at;
      slot = i;
    }
  }
  if (renderer->graphic_texture_cache[slot].texture) {
    renderer->gl.DeleteTextures(1, &renderer->graphic_texture_cache[slot].texture);
  }
  renderer->graphic_texture_cache[slot].texture = texture;
  renderer->graphic_texture_cache[slot].width = width;
  renderer->graphic_texture_cache[slot].height = height;
  renderer->graphic_texture_cache[slot].used_at = ++renderer->graphic_texture_tick;
  snprintf(renderer->graphic_texture_cache[slot].path,
           sizeof(renderer->graphic_texture_cache[slot].path), "%s", path);

  renderer->graphic_texture = texture;
  renderer->graphic_texture_width = width;
  renderer->graphic_texture_height = height;
  renderer->gl.BindTexture(GL_TEXTURE_2D, 0);
  renderer->gl.UseProgram(renderer->program);
  return 1;
}

static int plumos_mali_graphic_draw_texture(struct plumos_mali_renderer *renderer,
                                            const char *path, float x, float y,
                                            float w, float h) {
  float image_aspect;
  float target_aspect;
  float draw_w;
  float draw_h;
  float draw_x;
  float draw_y;
  GLfloat verts[8];
  GLfloat tex[8] = {
      0.0f, 0.0f,
      1.0f, 0.0f,
      0.0f, 1.0f,
      1.0f, 1.0f,
  };

  if (!plumos_mali_graphic_load_texture(renderer, path) ||
      renderer->graphic_texture_width <= 0 || renderer->graphic_texture_height <= 0) {
    return 0;
  }
  image_aspect = (float)renderer->graphic_texture_width /
                 (float)renderer->graphic_texture_height;
  target_aspect = w / h;
  draw_w = w;
  draw_h = h;
  if (image_aspect > target_aspect) {
    draw_h = w / image_aspect;
  } else {
    draw_w = h * image_aspect;
  }
  draw_x = x + (w - draw_w) * 0.5f;
  draw_y = y + (h - draw_h) * 0.5f;

  plumos_mali_point_to_ndc(renderer, draw_x, draw_y, &verts[0], &verts[1]);
  plumos_mali_point_to_ndc(renderer, draw_x + draw_w, draw_y, &verts[2], &verts[3]);
  plumos_mali_point_to_ndc(renderer, draw_x, draw_y + draw_h, &verts[4], &verts[5]);
  plumos_mali_point_to_ndc(renderer, draw_x + draw_w, draw_y + draw_h,
                           &verts[6], &verts[7]);

  renderer->gl.UseProgram(renderer->texture_program);
  renderer->gl.ActiveTexture(GL_TEXTURE0);
  renderer->gl.BindTexture(GL_TEXTURE_2D, renderer->graphic_texture);
  renderer->gl.Uniform1i(renderer->texture_sampler_uniform, 0);
  renderer->gl.Enable(GL_BLEND);
  renderer->gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  renderer->gl.EnableVertexAttribArray(0);
  renderer->gl.EnableVertexAttribArray(1);
  renderer->gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
  renderer->gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tex);
  renderer->gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  renderer->gl.DisableVertexAttribArray(1);
  renderer->gl.Disable(GL_BLEND);
  renderer->gl.BindTexture(GL_TEXTURE_2D, 0);
  renderer->gl.UseProgram(renderer->program);
  renderer->gl.EnableVertexAttribArray(0);
  return 1;
}

static int plumos_mali_graphic_draw_texture_cover(
    struct plumos_mali_renderer *renderer, const char *path,
    float x, float y, float w, float h) {
  float image_aspect;
  float target_aspect;
  float tex_left = 0.0f;
  float tex_right = 1.0f;
  float tex_top = 0.0f;
  float tex_bottom = 1.0f;
  GLfloat verts[8];
  GLfloat tex[8];

  if (!plumos_mali_graphic_load_texture(renderer, path) ||
      renderer->graphic_texture_width <= 0 || renderer->graphic_texture_height <= 0 ||
      w <= 0.0f || h <= 0.0f) {
    return 0;
  }
  image_aspect = (float)renderer->graphic_texture_width /
                 (float)renderer->graphic_texture_height;
  target_aspect = w / h;
  if (image_aspect > target_aspect) {
    float visible = target_aspect / image_aspect;
    tex_left = (1.0f - visible) * 0.5f;
    tex_right = tex_left + visible;
  } else if (image_aspect < target_aspect) {
    float visible = image_aspect / target_aspect;
    tex_top = (1.0f - visible) * 0.5f;
    tex_bottom = tex_top + visible;
  }

  tex[0] = tex_left;
  tex[1] = tex_top;
  tex[2] = tex_right;
  tex[3] = tex_top;
  tex[4] = tex_left;
  tex[5] = tex_bottom;
  tex[6] = tex_right;
  tex[7] = tex_bottom;

  plumos_mali_point_to_ndc(renderer, x, y, &verts[0], &verts[1]);
  plumos_mali_point_to_ndc(renderer, x + w, y, &verts[2], &verts[3]);
  plumos_mali_point_to_ndc(renderer, x, y + h, &verts[4], &verts[5]);
  plumos_mali_point_to_ndc(renderer, x + w, y + h, &verts[6], &verts[7]);

  renderer->gl.UseProgram(renderer->texture_program);
  renderer->gl.ActiveTexture(GL_TEXTURE0);
  renderer->gl.BindTexture(GL_TEXTURE_2D, renderer->graphic_texture);
  renderer->gl.Uniform1i(renderer->texture_sampler_uniform, 0);
  renderer->gl.Enable(GL_BLEND);
  renderer->gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  renderer->gl.EnableVertexAttribArray(0);
  renderer->gl.EnableVertexAttribArray(1);
  renderer->gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
  renderer->gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tex);
  renderer->gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  renderer->gl.DisableVertexAttribArray(1);
  renderer->gl.Disable(GL_BLEND);
  renderer->gl.BindTexture(GL_TEXTURE_2D, 0);
  renderer->gl.UseProgram(renderer->program);
  renderer->gl.EnableVertexAttribArray(0);
  return 1;
}
#endif

static void plumos_mali_rect_clipped_x(struct plumos_mali_renderer *renderer,
                                       float x, float y, float w, float h,
                                       float min_x, float max_x,
                                       float red, float green, float blue,
                                       float alpha) {
  if (max_x <= min_x) {
    return;
  }
  if (x < min_x) {
    w -= min_x - x;
    x = min_x;
  }
  if (x + w > max_x) {
    w = max_x - x;
  }
  if (w <= 0.0f || h <= 0.0f) {
    return;
  }
  plumos_mali_rect(renderer, x, y, w, h, red, green, blue, alpha);
}

static void plumos_mali_draw_builtin_char_clipped(struct plumos_mali_renderer *renderer,
                                                  unsigned char ch, float x, float y,
                                                  int scale, float min_x, float max_x,
                                                  float red, float green, float blue,
                                                  float alpha) {
  const unsigned char *rows;
  int row;

  if (scale <= 0) {
    return;
  }
  if (ch < 32 || ch > 126) {
    ch = '?';
  }
  rows = plumos_mali_glyph((char)ch);
  for (row = 0; row < 7; row++) {
    int col;
    for (col = 0; col < 5; col++) {
      if (rows[row] & (1u << (4 - col))) {
        plumos_mali_rect_clipped_x(renderer, x + (float)(col * scale),
                                   y + (float)(row * scale),
                                   (float)scale, (float)scale,
                                   min_x, max_x, red, green, blue, alpha);
      }
    }
  }
}

static void plumos_mali_draw_builtin_char(struct plumos_mali_renderer *renderer,
                                          unsigned char ch, float x, float y, int scale,
                                          float red, float green, float blue, float alpha) {
  plumos_mali_draw_builtin_char_clipped(renderer, ch, x, y, scale, 0.0f,
                                        (float)renderer->width, red, green, blue, alpha);
}

#ifdef PLUMOS_ENABLE_MALI_FREETYPE
static int plumos_mali_ft_set_size(struct plumos_mali_renderer *renderer, int scale) {
  int pixel_size;

  if (!renderer || !renderer->ft_ready || !renderer->ft_face) {
    return 0;
  }
  pixel_size = scale > 0 ? 9 * scale : 9;
  if (pixel_size < 12) {
    pixel_size = 12;
  }
  if (renderer->ft_pixel_size == pixel_size) {
    return 1;
  }
  if (FT_Set_Pixel_Sizes(renderer->ft_face, 0, (FT_UInt)pixel_size) != 0) {
    return 0;
  }
  renderer->ft_pixel_size = pixel_size;
  return 1;
}

static int plumos_mali_ft_advance(struct plumos_mali_renderer *renderer,
                                  unsigned int codepoint, int scale) {
  int fallback;
  int advance;
  size_t cache_index;
  struct plumos_mali_ft_advance_cache_entry *cached;

  fallback = 8 * scale;
  if (fallback < 8) {
    fallback = 8;
  }
  if (!renderer || !renderer->ft_ready || !renderer->ft_face) {
    return fallback;
  }
  cache_index = (((size_t)codepoint * 131u) + (size_t)scale) %
                PLUMOS_MALI_FT_ADVANCE_CACHE_SIZE;
  cached = &renderer->ft_advance_cache[cache_index];
  if (cached->valid && cached->codepoint == codepoint && cached->scale == scale) {
    return cached->advance;
  }
  if (!plumos_mali_ft_set_size(renderer, scale)) {
    return fallback;
  }
  if (FT_Load_Char(renderer->ft_face, (FT_ULong)codepoint, FT_LOAD_DEFAULT) != 0) {
    advance = fallback;
  } else {
    advance = (int)((renderer->ft_face->glyph->advance.x + 32) >> 6);
    if (advance <= 0) {
      advance = fallback;
    }
  }
  cached->codepoint = codepoint;
  cached->scale = scale;
  cached->advance = advance;
  cached->valid = 1;
  return advance;
}

static int plumos_mali_draw_freetype_codepoint_clipped(struct plumos_mali_renderer *renderer,
                                                       unsigned int codepoint,
                                                       float x, float y, int scale,
                                                       float min_x, float max_x,
                                                       float red, float green,
                                                       float blue, float alpha) {
  FT_GlyphSlot slot;
  FT_Bitmap *bitmap;
  int pixel_size;
  float baseline;
  int row;

  if (!plumos_mali_ft_set_size(renderer, scale)) {
    return 0;
  }
  if (FT_Load_Char(renderer->ft_face, (FT_ULong)codepoint,
                   FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT) != 0) {
    return 0;
  }
  slot = renderer->ft_face->glyph;
  bitmap = &slot->bitmap;
  pixel_size = renderer->ft_pixel_size > 0 ? renderer->ft_pixel_size : 9 * scale;
  baseline = y + (float)(pixel_size - 2);

  for (row = 0; row < (int)bitmap->rows; row++) {
    const unsigned char *row_data;
    int col;
    int span_start = -1;
    float row_y = baseline - (float)slot->bitmap_top + (float)row;
    if (bitmap->pitch < 0) {
      row_data = bitmap->buffer + ((int)bitmap->rows - 1 - row) * (-bitmap->pitch);
    } else {
      row_data = bitmap->buffer + row * bitmap->pitch;
    }
    for (col = 0; col <= (int)bitmap->width; col++) {
      unsigned char value = 0;
      int lit = 0;
      if (col < (int)bitmap->width) {
        if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
          value = (row_data[col >> 3] & (0x80u >> (col & 7))) ? 255 : 0;
        } else {
          value = row_data[col];
        }
        lit = value >= 80;
      }
      if (lit) {
        if (span_start < 0) {
          span_start = col;
        }
      } else if (span_start >= 0) {
        plumos_mali_rect_clipped_x(renderer,
                                   x + (float)slot->bitmap_left + (float)span_start,
                                   row_y, (float)(col - span_start), 1.0f,
                                   min_x, max_x,
                                   red, green, blue, alpha);
        span_start = -1;
      }
    }
  }
  return 1;
}

static int plumos_mali_draw_freetype_codepoint(struct plumos_mali_renderer *renderer,
                                               unsigned int codepoint, float x, float y,
                                               int scale, float red, float green,
                                               float blue, float alpha) {
  return plumos_mali_draw_freetype_codepoint_clipped(renderer, codepoint, x, y,
                                                     scale, 0.0f,
                                                     (float)renderer->width,
                                                     red, green, blue, alpha);
}
#endif

static int plumos_mali_codepoint_advance(struct plumos_mali_renderer *renderer,
                                         unsigned int codepoint, int scale,
                                         int prefer_freetype) {
  int ascii_step;

  ascii_step = 6 * scale;
  if (ascii_step < 6) {
    ascii_step = 6;
  }
#ifdef PLUMOS_ENABLE_MALI_FREETYPE
  if (renderer && renderer->ft_ready && renderer->ft_face &&
      (prefer_freetype || codepoint > 126)) {
    return plumos_mali_ft_advance(renderer, codepoint, scale);
  }
#else
  (void)renderer;
  (void)prefer_freetype;
#endif
  return ascii_step;
}

static int plumos_mali_text_width_font(struct plumos_mali_renderer *renderer,
                                       const char *text, int scale,
                                       int prefer_freetype) {
  const char *p;
  int width = 0;

  if (!text || scale <= 0) {
    return 0;
  }
  p = text;
  while (*p) {
    unsigned int cp;
    const char *next = plumos_mali_utf8_next(p, &cp);
    if (cp == '\t') {
      cp = ' ';
    }
    if (cp >= 32) {
      width += plumos_mali_codepoint_advance(renderer, cp, scale, prefer_freetype);
    }
    p = next;
  }
  return width;
}

static long long plumos_mali_time_ms(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    return (long long)time(NULL) * 1000LL;
  }
  return (long long)tv.tv_sec * 1000LL + (long long)(tv.tv_usec / 1000);
}

static int plumos_mali_marquee_offset(struct plumos_mali_renderer *renderer,
                                      int text_width, int available_width,
                                      int step_px) {
  int overflow;
  int scroll_ms;
  int cycle_ms;
  int phase_ms;
  long long elapsed_ms;
  const int hold_ms = 1000;
  const int pixels_per_second = 80;

  (void)step_px;
  overflow = text_width - available_width;
  if (overflow <= 0) {
    return 0;
  }
  elapsed_ms = plumos_mali_time_ms() - (renderer ? renderer->marquee_focus_ms : 0);
  if (elapsed_ms < hold_ms) {
    return 0;
  }
  scroll_ms = (overflow * 1000 + pixels_per_second - 1) / pixels_per_second;
  cycle_ms = scroll_ms + hold_ms;
  phase_ms = (int)((elapsed_ms - hold_ms) % (long long)cycle_ms);
  if (phase_ms >= scroll_ms) {
    return overflow;
  }
  return (phase_ms * pixels_per_second) / 1000;
}

static void plumos_mali_text_limited(struct plumos_mali_renderer *renderer,
                                     const char *text, float x, float y, int scale,
                                     int prefer_freetype, float max_x,
                                     float red, float green, float blue,
                                     float alpha) {
  const char *p;
  float pen_x = 0.0f;
  int ascii_step = 6 * scale;

  if (!text || scale <= 0) {
    return;
  }
  if (max_x <= 0.0f || max_x > (float)(renderer->width - 8)) {
    max_x = (float)(renderer->width - 8);
  }
  if (x >= max_x) {
    return;
  }
  p = text;
  while (*p) {
    unsigned int cp;
    int advance = ascii_step;
    const char *next = plumos_mali_utf8_next(p, &cp);

    if (cp == '\t') {
      cp = ' ';
    }
    if (cp < 32) {
      p = next;
      continue;
    }
    if (cp <= 126) {
      advance = ascii_step;
#ifdef PLUMOS_ENABLE_MALI_FREETYPE
      if (prefer_freetype && renderer->ft_ready && renderer->ft_face) {
        advance = plumos_mali_ft_advance(renderer, cp, scale);
      }
#endif
      if (x + pen_x + (float)advance > max_x) {
        break;
      }
#ifdef PLUMOS_ENABLE_MALI_FREETYPE
      if (prefer_freetype && renderer->ft_ready && renderer->ft_face) {
        if (!plumos_mali_draw_freetype_codepoint(renderer, cp, x + pen_x, y, scale,
                                                 red, green, blue, alpha)) {
          plumos_mali_draw_builtin_char(renderer, (unsigned char)cp, x + pen_x, y, scale,
                                        red, green, blue, alpha);
          advance = ascii_step;
        }
      } else
#endif
      {
        plumos_mali_draw_builtin_char(renderer, (unsigned char)cp, x + pen_x, y, scale,
                                      red, green, blue, alpha);
      }
    } else {
#ifdef PLUMOS_ENABLE_MALI_FREETYPE
      advance = plumos_mali_ft_advance(renderer, cp, scale);
      if (x + pen_x + (float)advance > max_x) {
        break;
      }
      if (!plumos_mali_draw_freetype_codepoint(renderer, cp, x + pen_x, y, scale,
                                               red, green, blue, alpha)) {
        plumos_mali_draw_builtin_char(renderer, '?', x + pen_x, y, scale,
                                      red, green, blue, alpha);
        advance = ascii_step;
      }
#else
      if (x + pen_x + (float)ascii_step > max_x) {
        break;
      }
      plumos_mali_draw_builtin_char(renderer, '?', x + pen_x, y, scale,
                                    red, green, blue, alpha);
      advance = ascii_step;
#endif
    }
    pen_x += (float)advance;
    p = next;
  }
}

static void plumos_mali_text_clipped(struct plumos_mali_renderer *renderer,
                                     const char *text, float x, float y, int scale,
                                     int prefer_freetype, float min_x, float max_x,
                                     float red, float green, float blue,
                                     float alpha) {
  const char *p;
  float pen_x = 0.0f;

  if (!text || scale <= 0 || max_x <= min_x) {
    return;
  }
  if (max_x > (float)(renderer->width - 8)) {
    max_x = (float)(renderer->width - 8);
  }
  p = text;
  while (*p) {
    unsigned int cp;
    int advance;
    const char *next = plumos_mali_utf8_next(p, &cp);
    float glyph_x = x + pen_x;

    if (cp == '\t') {
      cp = ' ';
    }
    if (cp < 32) {
      p = next;
      continue;
    }
    advance = plumos_mali_codepoint_advance(renderer, cp, scale, prefer_freetype);
    if (glyph_x >= max_x) {
      break;
    }
    if (glyph_x + (float)advance > min_x) {
      if (cp <= 126) {
#ifdef PLUMOS_ENABLE_MALI_FREETYPE
        if (prefer_freetype && renderer->ft_ready && renderer->ft_face) {
          if (!plumos_mali_draw_freetype_codepoint_clipped(renderer, cp, glyph_x,
                                                           y, scale, min_x, max_x,
                                                           red, green, blue, alpha)) {
            plumos_mali_draw_builtin_char_clipped(renderer, (unsigned char)cp,
                                                  glyph_x, y, scale, min_x, max_x,
                                                  red, green, blue, alpha);
          }
        } else
#endif
        {
          plumos_mali_draw_builtin_char_clipped(renderer, (unsigned char)cp,
                                                glyph_x, y, scale, min_x, max_x,
                                                red, green, blue, alpha);
        }
      } else {
#ifdef PLUMOS_ENABLE_MALI_FREETYPE
        if (!plumos_mali_draw_freetype_codepoint_clipped(renderer, cp, glyph_x,
                                                         y, scale, min_x, max_x,
                                                         red, green, blue, alpha)) {
          plumos_mali_draw_builtin_char_clipped(renderer, '?', glyph_x, y, scale,
                                                min_x, max_x, red, green, blue, alpha);
        }
#else
        plumos_mali_draw_builtin_char_clipped(renderer, '?', glyph_x, y, scale,
                                              min_x, max_x, red, green, blue, alpha);
#endif
      }
    }
    pen_x += (float)advance;
    p = next;
  }
}

static void plumos_mali_text(struct plumos_mali_renderer *renderer, const char *text,
                             float x, float y, int scale, float red, float green,
                             float blue, float alpha) {
  plumos_mali_text_limited(renderer, text, x, y, scale, 0, 0.0f,
                           red, green, blue, alpha);
}

static int plumos_mali_text_width(const char *text, int scale) {
  if (!text || scale <= 0) {
    return 0;
  }
  return (int)strlen(text) * 6 * scale;
}

static int plumos_mali_read_first_line(const char *path, char *out, size_t out_size) {
  FILE *f;
  size_t len;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  f = fopen(path, "r");
  if (!f) {
    return 0;
  }
  if (!fgets(out, (int)out_size, f)) {
    fclose(f);
    out[0] = '\0';
    return 0;
  }
  fclose(f);
  len = strlen(out);
  while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r' || out[len - 1] == ' ')) {
    out[--len] = '\0';
  }
  return out[0] != '\0';
}

static void plumos_mali_time_label(char *out, size_t out_size) {
  time_t now;
  struct tm tm_now;

  if (!out || out_size == 0) {
    return;
  }
  now = time(NULL);
  if (now == (time_t)-1 || !localtime_r(&now, &tm_now) ||
      strftime(out, out_size, "%H:%M", &tm_now) == 0) {
    snprintf(out, out_size, "--:--");
  }
}

static void plumos_mali_wifi_label(char *out, size_t out_size) {
  FILE *f;
  char line[256];
  int linked = 0;

  if (!out || out_size == 0) {
    return;
  }
  f = fopen("/proc/net/wireless", "r");
  if (f) {
    while (fgets(line, sizeof(line), f)) {
      unsigned int status = 0;
      float quality = 0.0f;
      if (sscanf(line, " wlan0: %x %f", &status, &quality) == 2 && quality > 0.0f) {
        linked = 1;
        break;
      }
    }
    fclose(f);
  }
  snprintf(out, out_size, "%s", linked ? "WIFI" : "NO WIFI");
}

static void plumos_mali_battery_label(char *out, size_t out_size) {
  char capacity[32];
  char status[32];
  const char *prefix = "BAT";

  if (!out || out_size == 0) {
    return;
  }
  if (!plumos_mali_read_first_line("/sys/class/power_supply/battery/capacity",
                                   capacity, sizeof(capacity))) {
    snprintf(out, out_size, "BAT --");
    return;
  }
  if (plumos_mali_read_first_line("/sys/class/power_supply/battery/status",
                                  status, sizeof(status)) &&
      (strcmp(status, "Charging") == 0 || strcmp(status, "Full") == 0)) {
    prefix = "CHG";
  }
  snprintf(out, out_size, "%s %.3s", prefix, capacity);
}

static int plumos_mali_starts_with(const char *s, const char *prefix) {
  return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}

static int plumos_mali_is_empty_or_help(const char *line) {
  if (!line || !line[0]) {
    return 1;
  }
  if (strstr(line, "A:") && strstr(line, "Q:")) {
    return 1;
  }
  return 0;
}

static int plumos_mali_is_entry_line(const char *line) {
  const char *p = line;

  if (!line || !line[0]) {
    return 0;
  }
  if (*p == '>') {
    return 1;
  }
  while (*p == ' ') {
    p++;
  }
  return *p >= '0' && *p <= '9';
}

static void plumos_mali_compact_spaces(const char *in, char *out, size_t out_size) {
  size_t n = 0;
  int pending_space = 0;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!in) {
    return;
  }
  while (*in == ' ') {
    in++;
  }
  while (*in && n + 1 < out_size) {
    unsigned char c = (unsigned char)*in++;
    if (c <= ' ') {
      pending_space = n > 0;
      continue;
    }
    if (pending_space && n + 1 < out_size) {
      out[n++] = ' ';
      pending_space = 0;
    }
    out[n++] = (char)c;
  }
  while (n > 0 && out[n - 1] == ' ') {
    n--;
  }
  out[n] = '\0';
}

static void plumos_mali_truncate(char *s, size_t max_chars) {
  const char *p;
  size_t last_ok = 0;
  size_t cells = 0;

  if (!s || max_chars == 0) {
    return;
  }
  p = s;
  while (*p) {
    unsigned int cp;
    const char *next = plumos_mali_utf8_next(p, &cp);
    size_t width = (size_t)plumos_mali_utf8_cell_width(cp);
    if (cells + width > max_chars) {
      break;
    }
    cells += width;
    last_ok = (size_t)(next - s);
    p = next;
  }
  if (s[last_ok] == '\0') {
    return;
  }
  if (max_chars > 1 && last_ok + 2 <= strlen(s) + 1) {
    s[last_ok] = '~';
    s[last_ok + 1] = '\0';
  } else {
    s[last_ok] = '\0';
  }
}

static void plumos_mali_make_title(const char *line, char *out, size_t out_size) {
  const char *screen;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  screen = line ? strstr(line, " - ") : NULL;
  if (screen && screen[3]) {
    snprintf(out, out_size, "%s", screen + 3);
  } else if (line && line[0]) {
    snprintf(out, out_size, "%.40s", line);
  } else {
    snprintf(out, out_size, "plumOS");
  }
}

static void plumos_mali_trim_right(char *s) {
  size_t len;

  if (!s) {
    return;
  }
  len = strlen(s);
  while (len > 0 && s[len - 1] == ' ') {
    s[--len] = '\0';
  }
}

static void plumos_mali_ascii_upper_inplace(char *s) {
  unsigned char *p = (unsigned char *)s;

  if (!p) {
    return;
  }
  while (*p) {
    if (*p >= 'a' && *p <= 'z') {
      *p = (unsigned char)(*p - 'a' + 'A');
    }
    p++;
  }
}

static int plumos_mali_entry_head(const char *line, char *marker, char *number,
                                  size_t number_size, const char **rest) {
  const char *p = line;
  size_t n = 0;

  if (!line || !number || number_size == 0 || !rest) {
    return 0;
  }
  *marker = ' ';
  number[0] = '\0';
  if (*p == '>') {
    *marker = '>';
    p++;
  }
  while (*p == ' ') {
    p++;
  }
  while (*p >= '0' && *p <= '9' && n + 1 < number_size) {
    number[n++] = *p++;
  }
  number[n] = '\0';
  while (*p == ' ') {
    p++;
  }
  *rest = p;
  return number[0] != '\0' && *rest && **rest;
}

static int plumos_mali_make_rom_entry(const char *line, char *out, size_t out_size) {
  char marker;
  char number[16];
  char title[128];
  const char *rest;

  if (!plumos_mali_entry_head(line, &marker, number, sizeof(number), &rest)) {
    return 0;
  }
  memset(title, 0, sizeof(title));
  plumos_mali_copy_utf8_cells(rest, title, sizeof(title), 120);
  plumos_mali_trim_right(title);
  if (!title[0]) {
    return 0;
  }
  snprintf(out, out_size, "%c %s %s", marker, number, title);
  return 1;
}

static int plumos_mali_make_safe_entry(const char *line, char *out, size_t out_size) {
  char marker;
  char number[16];
  char action[32];
  const char *rest;
  size_t n = 0;

  if (!plumos_mali_entry_head(line, &marker, number, sizeof(number), &rest)) {
    return 0;
  }
  while (rest[n] && rest[n] != ' ' && n + 1 < sizeof(action)) {
    action[n] = rest[n];
    n++;
  }
  action[n] = '\0';
  if (!action[0]) {
    return 0;
  }
  snprintf(out, out_size, "%c %s %s", marker, number, action);
  return 1;
}

static void plumos_mali_make_entry(const char *line, const char *screen_title,
                                   char *out, size_t out_size) {
  char compact[160];
  char *profile;

  if (!out || out_size == 0) {
    return;
  }
  if ((strstr(screen_title, "ROMS") || strstr(screen_title, "FAVORITES") ||
       strstr(screen_title, "RECENT")) &&
      plumos_mali_make_rom_entry(line, out, out_size)) {
    return;
  }
  if (strstr(screen_title, "SAFE") && plumos_mali_make_safe_entry(line, out, out_size)) {
    return;
  }
  plumos_mali_compact_spaces(line, compact, sizeof(compact));
  profile = strstr(compact, " profile=");
  if (profile) {
    *profile = '\0';
  }
  snprintf(out, out_size, "%s", compact);
  if (strstr(screen_title, "START") || strstr(screen_title, "Apps") ||
      strstr(screen_title, "APPS") || strstr(screen_title, "Settings") ||
      strstr(screen_title, "SETTINGS") || strstr(screen_title, "HELP") ||
      strstr(screen_title, "Thumbnail Results") ||
      strstr(screen_title, "Scraping")) {
    plumos_mali_truncate(out, 80);
  } else {
    plumos_mali_truncate(out, 38);
  }
}

static void plumos_mali_make_meta(const char *line, char *out, size_t out_size) {
  char compact[160];

  if (!out || out_size == 0 || !line || !line[0]) {
    return;
  }
  plumos_mali_compact_spaces(line, compact, sizeof(compact));
  snprintf(out, out_size, "%s", compact);
  plumos_mali_truncate(out, 38);
}

static void plumos_mali_find_prompt_path(const char lines[][PLUMOS_MALI_RENDER_LINE_MAX],
                                         size_t line_count, char *out,
                                         size_t out_size) {
  size_t i;
  const char *prefix = "prompt_path=";
  size_t prefix_len = strlen(prefix);

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  for (i = 0; i < line_count; i++) {
    if (strncmp(lines[i], prefix, prefix_len) == 0 && lines[i][prefix_len]) {
      snprintf(out, out_size, "%s", lines[i] + prefix_len);
      return;
    }
  }
}

static void plumos_mali_find_footer_lines(const char lines[][PLUMOS_MALI_RENDER_LINE_MAX],
                                          size_t line_count,
                                          char *line1, size_t line1_size,
                                          char *line2, size_t line2_size) {
  size_t i;
  const char *prefix1 = "footer1=";
  const char *prefix2 = "footer2=";
  size_t prefix1_len = strlen(prefix1);
  size_t prefix2_len = strlen(prefix2);

  if (line1 && line1_size > 0) {
    line1[0] = '\0';
  }
  if (line2 && line2_size > 0) {
    line2[0] = '\0';
  }
  for (i = 0; i < line_count; i++) {
    if (line1 && line1_size > 0 &&
        strncmp(lines[i], prefix1, prefix1_len) == 0) {
      snprintf(line1, line1_size, "%s", lines[i] + prefix1_len);
      plumos_mali_truncate(line1, 50);
    } else if (line2 && line2_size > 0 &&
               strncmp(lines[i], prefix2, prefix2_len) == 0) {
      snprintf(line2, line2_size, "%s", lines[i] + prefix2_len);
      plumos_mali_truncate(line2, 50);
    }
  }
}

static int plumos_mali_find_wifi_keyboard_cursor(
    const char lines[][PLUMOS_MALI_RENDER_LINE_MAX],
    size_t line_count, int *row_out, int *col_out) {
  size_t i;
  const char *prefix = "wifi_keyboard_cursor=";
  size_t prefix_len = strlen(prefix);

  if (row_out) {
    *row_out = -1;
  }
  if (col_out) {
    *col_out = -1;
  }
  for (i = 0; i < line_count; i++) {
    char *endptr;
    long row;
    long col;

    if (strncmp(lines[i], prefix, prefix_len) != 0) {
      continue;
    }
    row = strtol(lines[i] + prefix_len, &endptr, 10);
    if (endptr == lines[i] + prefix_len || *endptr != ',') {
      return 0;
    }
    col = strtol(endptr + 1, &endptr, 10);
    if (row < 0 || col < 0) {
      return 0;
    }
    if (row_out) {
      *row_out = (int)row;
    }
    if (col_out) {
      *col_out = (int)col;
    }
    return 1;
  }
  return 0;
}

static void plumos_mali_find_prefixed_line(
    const char lines[][PLUMOS_MALI_RENDER_LINE_MAX],
    size_t line_count, const char *prefix, char *out, size_t out_size) {
  size_t i;
  size_t prefix_len;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!prefix) {
    return;
  }
  prefix_len = strlen(prefix);
  for (i = 0; i < line_count; i++) {
    if (strncmp(lines[i], prefix, prefix_len) == 0) {
      snprintf(out, out_size, "%s", lines[i] + prefix_len);
      plumos_mali_truncate(out, 64);
      return;
    }
  }
}

static int plumos_mali_has_prefixed_line(
    const char lines[][PLUMOS_MALI_RENDER_LINE_MAX],
    size_t line_count, const char *prefix) {
  size_t i;
  size_t prefix_len;

  if (!prefix) {
    return 0;
  }
  prefix_len = strlen(prefix);
  for (i = 0; i < line_count; i++) {
    if (strncmp(lines[i], prefix, prefix_len) == 0) {
      return 1;
    }
  }
  return 0;
}

struct plumos_mali_graphic_entry {
  int selected;
  char title[PLUMOS_MALI_RENDER_LINE_MAX];
  char detail[PLUMOS_MALI_RENDER_LINE_MAX];
  char thumbnail[PLUMOS_MALI_RENDER_LINE_MAX];
};

struct plumos_mali_rgb {
  float r;
  float g;
  float b;
};

struct plumos_mali_graphic_theme {
  struct plumos_mali_rgb background;
  struct plumos_mali_rgb foreground;
  struct plumos_mali_rgb muted;
  struct plumos_mali_rgb accent;
  struct plumos_mali_rgb panel;
  struct plumos_mali_rgb panel_inner;
  struct plumos_mali_rgb media_panel;
  struct plumos_mali_rgb selection_background;
  struct plumos_mali_rgb selection_foreground;
  struct plumos_mali_rgb danger;
  char background_image[PLUMOS_MALI_RENDER_LINE_MAX];
  char gallery_background_image[PLUMOS_MALI_RENDER_LINE_MAX];
  char placeholder_thumbnail[PLUMOS_MALI_RENDER_LINE_MAX];
  char top_layout[32];
  char transition_axis[32];
  char transition_easing[32];
};

static void plumos_mali_graphic_copy_field(char *out, size_t out_size,
                                           const char *start, const char *end) {
  size_t len;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!start) {
    return;
  }
  if (!end) {
    end = start + strlen(start);
  }
  len = (size_t)(end - start);
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, start, len);
  out[len] = '\0';
}

static struct plumos_mali_rgb plumos_mali_rgb(float r, float g, float b) {
  struct plumos_mali_rgb color;
  color.r = r;
  color.g = g;
  color.b = b;
  return color;
}

static void plumos_mali_graphic_theme_defaults(
    struct plumos_mali_graphic_theme *theme) {
  if (!theme) {
    return;
  }
  memset(theme, 0, sizeof(*theme));
  theme->background = plumos_mali_rgb(0.010f, 0.012f, 0.013f);
  theme->foreground = plumos_mali_rgb(0.72f, 0.80f, 0.78f);
  theme->muted = plumos_mali_rgb(0.52f, 0.66f, 0.66f);
  theme->accent = plumos_mali_rgb(1.0f, 0.52f, 0.05f);
  theme->panel = plumos_mali_rgb(0.12f, 0.17f, 0.18f);
  theme->panel_inner = plumos_mali_rgb(0.020f, 0.026f, 0.028f);
  theme->media_panel = plumos_mali_rgb(0.11f, 0.16f, 0.18f);
  theme->selection_background = plumos_mali_rgb(0.14f, 0.23f, 0.20f);
  theme->selection_foreground = plumos_mali_rgb(1.0f, 0.90f, 0.48f);
  theme->danger = plumos_mali_rgb(1.0f, 0.08f, 0.04f);
  snprintf(theme->top_layout, sizeof(theme->top_layout), "%s", "tile_grid");
  snprintf(theme->transition_axis, sizeof(theme->transition_axis), "%s", "vertical");
  snprintf(theme->transition_easing, sizeof(theme->transition_easing), "%s",
           "smoothstep");
}

static int plumos_mali_hex_digit(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

static int plumos_mali_parse_hex_color(const char *value,
                                       struct plumos_mali_rgb *out) {
  int r;
  int g;
  int b;

  if (!value || !out || value[0] != '#' || strlen(value) != 7) {
    return 0;
  }
  if (!isxdigit((unsigned char)value[1]) ||
      !isxdigit((unsigned char)value[2]) ||
      !isxdigit((unsigned char)value[3]) ||
      !isxdigit((unsigned char)value[4]) ||
      !isxdigit((unsigned char)value[5]) ||
      !isxdigit((unsigned char)value[6])) {
    return 0;
  }
  r = plumos_mali_hex_digit(value[1]) * 16 + plumos_mali_hex_digit(value[2]);
  g = plumos_mali_hex_digit(value[3]) * 16 + plumos_mali_hex_digit(value[4]);
  b = plumos_mali_hex_digit(value[5]) * 16 + plumos_mali_hex_digit(value[6]);
  out->r = (float)r / 255.0f;
  out->g = (float)g / 255.0f;
  out->b = (float)b / 255.0f;
  return 1;
}

static void plumos_mali_graphic_theme_set_color(
    struct plumos_mali_graphic_theme *theme, const char *key, const char *value) {
  struct plumos_mali_rgb color;

  if (!theme || !key || !value ||
      !plumos_mali_parse_hex_color(value, &color)) {
    return;
  }
  if (strcmp(key, "background") == 0) {
    theme->background = color;
  } else if (strcmp(key, "foreground") == 0) {
    theme->foreground = color;
  } else if (strcmp(key, "muted") == 0) {
    theme->muted = color;
  } else if (strcmp(key, "accent") == 0) {
    theme->accent = color;
  } else if (strcmp(key, "panel") == 0) {
    theme->panel = color;
  } else if (strcmp(key, "panel_inner") == 0) {
    theme->panel_inner = color;
  } else if (strcmp(key, "media_panel") == 0) {
    theme->media_panel = color;
  } else if (strcmp(key, "selection_background") == 0) {
    theme->selection_background = color;
  } else if (strcmp(key, "selection_foreground") == 0) {
    theme->selection_foreground = color;
  } else if (strcmp(key, "danger") == 0) {
    theme->danger = color;
  }
}

static void plumos_mali_graphic_theme_parse_line(
    struct plumos_mali_graphic_theme *theme, const char *line) {
  const char *prefix_color = "graphic_theme_color\t";
  const char *prefix_asset = "graphic_theme_asset\t";
  const char *prefix_motion = "graphic_theme_motion\t";
  const char *p;
  const char *tab;
  char key[64];
  char value[PLUMOS_MALI_RENDER_LINE_MAX];

  if (!theme || !line) {
    return;
  }
  if (plumos_mali_starts_with(line, prefix_color)) {
    p = line + strlen(prefix_color);
  } else if (plumos_mali_starts_with(line, prefix_asset)) {
    p = line + strlen(prefix_asset);
  } else if (plumos_mali_starts_with(line, prefix_motion)) {
    p = line + strlen(prefix_motion);
  } else {
    return;
  }
  tab = strchr(p, '\t');
  if (!tab) {
    return;
  }
  plumos_mali_graphic_copy_field(key, sizeof(key), p, tab);
  plumos_mali_graphic_copy_field(value, sizeof(value), tab + 1, NULL);
  if (plumos_mali_starts_with(line, prefix_color)) {
    plumos_mali_graphic_theme_set_color(theme, key, value);
  } else if (plumos_mali_starts_with(line, prefix_motion)) {
    if (strcmp(key, "top_layout") == 0 &&
        (strcmp(value, "tile_grid") == 0 || strcmp(value, "tile_strip") == 0)) {
      snprintf(theme->top_layout, sizeof(theme->top_layout), "%s",
               strcmp(value, "tile_strip") == 0 ? "tile_strip" : "tile_grid");
    } else if (strcmp(key, "transition_axis") == 0 &&
        (strcmp(value, "vertical") == 0 || strcmp(value, "horizontal") == 0)) {
      snprintf(theme->transition_axis, sizeof(theme->transition_axis), "%s",
               strcmp(value, "horizontal") == 0 ? "horizontal" : "vertical");
    } else if (strcmp(key, "transition_easing") == 0 &&
               (strcmp(value, "smoothstep") == 0 || strcmp(value, "linear") == 0)) {
      snprintf(theme->transition_easing, sizeof(theme->transition_easing), "%s",
               strcmp(value, "linear") == 0 ? "linear" : "smoothstep");
    }
  } else if (strcmp(key, "background") == 0) {
    snprintf(theme->background_image, sizeof(theme->background_image),
             "%s", value);
  } else if (strcmp(key, "gallery_background") == 0) {
    snprintf(theme->gallery_background_image,
             sizeof(theme->gallery_background_image), "%s", value);
  } else if (strcmp(key, "placeholder") == 0) {
    snprintf(theme->placeholder_thumbnail, sizeof(theme->placeholder_thumbnail),
             "%s", value);
  }
}

static void plumos_mali_graphic_theme_load(
    const char lines[][PLUMOS_MALI_RENDER_LINE_MAX], size_t line_count,
    struct plumos_mali_graphic_theme *theme) {
  size_t i;

  plumos_mali_graphic_theme_defaults(theme);
  for (i = 0; i < line_count; i++) {
    plumos_mali_graphic_theme_parse_line(theme, lines[i]);
  }
}

static void plumos_mali_graphic_rect(
    struct plumos_mali_renderer *renderer, float x, float y, float w, float h,
    struct plumos_mali_rgb color, float alpha) {
  plumos_mali_rect(renderer, x, y, w, h, color.r, color.g, color.b, alpha);
}

static void plumos_mali_graphic_text(
    struct plumos_mali_renderer *renderer, const char *text, float x, float y,
    int scale, struct plumos_mali_rgb color, float alpha) {
  plumos_mali_text(renderer, text, x, y, scale, color.r, color.g, color.b, alpha);
}

static void plumos_mali_graphic_text_clipped(
    struct plumos_mali_renderer *renderer, const char *text, float x, float y,
    int scale, int use_font, float clip_left, float clip_right,
    struct plumos_mali_rgb color, float alpha) {
  plumos_mali_text_clipped(renderer, text, x, y, scale, use_font, clip_left,
                           clip_right, color.r, color.g, color.b, alpha);
}

static void plumos_mali_graphic_normalize_text_field(char *text, size_t text_size) {
  char normalized[PLUMOS_MALI_RENDER_LINE_MAX];

  if (!text || text_size == 0 || !text[0]) {
    return;
  }
  plumos_mali_normalize_kana_marks(text, normalized, sizeof(normalized));
  if (normalized[0] && strcmp(normalized, text) != 0) {
    snprintf(text, text_size, "%s", normalized);
  }
}

static int plumos_mali_graphic_parse_entry_with_prefix(
    const char *line, const char *prefix,
    struct plumos_mali_graphic_entry *entry) {
  const char *p;
  const char *tab;
  const char *tab2;
  char *endptr;
  long selected;

  if (!line || !entry || !plumos_mali_starts_with(line, prefix)) {
    return 0;
  }
  memset(entry, 0, sizeof(*entry));
  p = line + strlen(prefix);
  selected = strtol(p, &endptr, 10);
  if (endptr == p || *endptr != '\t') {
    return 0;
  }
  entry->selected = selected != 0;
  p = endptr + 1;
  tab = strchr(p, '\t');
  if (!tab) {
    plumos_mali_graphic_copy_field(entry->title, sizeof(entry->title), p, NULL);
    plumos_mali_graphic_normalize_text_field(entry->title, sizeof(entry->title));
    return entry->title[0] != '\0';
  }
  plumos_mali_graphic_copy_field(entry->title, sizeof(entry->title), p, tab);
  plumos_mali_graphic_normalize_text_field(entry->title, sizeof(entry->title));
  tab2 = strchr(tab + 1, '\t');
  if (!tab2) {
    plumos_mali_graphic_copy_field(entry->detail, sizeof(entry->detail), tab + 1, NULL);
    plumos_mali_graphic_normalize_text_field(entry->detail, sizeof(entry->detail));
  } else {
    plumos_mali_graphic_copy_field(entry->detail, sizeof(entry->detail), tab + 1, tab2);
    plumos_mali_graphic_normalize_text_field(entry->detail, sizeof(entry->detail));
    plumos_mali_graphic_copy_field(entry->thumbnail, sizeof(entry->thumbnail), tab2 + 1,
                                   NULL);
  }
  return entry->title[0] != '\0';
}

static int plumos_mali_graphic_parse_entry(
    const char *line, struct plumos_mali_graphic_entry *entry) {
  return plumos_mali_graphic_parse_entry_with_prefix(
      line, "graphic_entry\t", entry);
}

static int plumos_mali_graphic_parse_prev_entry(
    const char *line, struct plumos_mali_graphic_entry *entry) {
  return plumos_mali_graphic_parse_entry_with_prefix(
      line, "graphic_prev_entry\t", entry);
}

static void plumos_mali_graphic_initials(const char *title, char *out, size_t out_size) {
  const unsigned char *p = (const unsigned char *)title;
  size_t n = 0;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  while (p && *p && n + 1 < out_size && n < 3) {
    if ((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) {
      out[n++] = (char)*p;
    } else if (*p >= 'a' && *p <= 'z') {
      out[n++] = (char)(*p - 'a' + 'A');
    } else if (*p & 0x80) {
      break;
    }
    p++;
  }
  out[n] = '\0';
  if (!out[0]) {
    snprintf(out, out_size, "ROM");
  }
}

static void plumos_mali_graphic_top_bar(struct plumos_mali_renderer *renderer,
                                        const struct plumos_mali_graphic_theme *theme,
                                        const char *title) {
  char time_label[16];
  char wifi_label[16];
  char battery_label[24];
  char right[64];
  int right_width;

  plumos_mali_time_label(time_label, sizeof(time_label));
  plumos_mali_wifi_label(wifi_label, sizeof(wifi_label));
  plumos_mali_battery_label(battery_label, sizeof(battery_label));
  snprintf(right, sizeof(right), "%s  %s  %s", time_label, wifi_label, battery_label);
  right_width = plumos_mali_text_width(right, 2);

  plumos_mali_graphic_rect(renderer, 0.0f, 0.0f, (float)renderer->width, 40.0f,
                           theme->panel_inner, 1.0f);
  plumos_mali_graphic_rect(renderer, 0.0f, 40.0f, (float)renderer->width, 2.0f,
                           theme->panel, 1.0f);
  plumos_mali_graphic_text(renderer, title && title[0] ? title : "PLUMOS A30",
                           16.0f, 12.0f, 2, theme->foreground, 1.0f);
  plumos_mali_graphic_text(renderer, right,
                           (float)(renderer->width - 14 - right_width),
                           12.0f, 2, theme->muted, 1.0f);
}

static void plumos_mali_graphic_top_layout_metrics(
    const struct plumos_mali_renderer *renderer,
    const struct plumos_mali_graphic_theme *theme,
    size_t *columns_out, size_t *rows_out, float *grid_x_out, float *grid_y_out,
    float *tile_size_out, float *gap_out) {
  int strip = theme && strcmp(theme->top_layout, "tile_strip") == 0;
  size_t columns = strip ? 2u : 3u;
  size_t rows = strip ? 1u : 2u;
  float horizontal_margin = strip ? 32.0f : 22.0f;
  float grid_y = strip ? 116.0f : 70.0f;
  float bottom_margin = strip ? 42.0f : 18.0f;
  float gap = strip ? 16.0f : 10.0f;
  float width_limited =
      ((float)renderer->width - horizontal_margin * 2.0f -
       gap * (float)(columns - 1)) /
      (float)columns;
  float height_limited =
      ((float)renderer->height - grid_y - bottom_margin -
       gap * (float)(rows - 1)) /
      (float)rows;
  float tile_size = width_limited < height_limited ? width_limited
                                                   : height_limited;
  float grid_width = tile_size * (float)columns +
                     gap * (float)(columns - 1);
  float grid_x = ((float)renderer->width - grid_width) * 0.5f;

  if (strip && tile_size > 260.0f) {
    tile_size = 260.0f;
    grid_width = tile_size * (float)columns + gap * (float)(columns - 1);
    grid_x = ((float)renderer->width - grid_width) * 0.5f;
  }
  if (grid_x < 8.0f) {
    grid_x = 8.0f;
  }
  if (columns_out) {
    *columns_out = columns;
  }
  if (rows_out) {
    *rows_out = rows;
  }
  if (grid_x_out) {
    *grid_x_out = grid_x;
  }
  if (grid_y_out) {
    *grid_y_out = grid_y;
  }
  if (tile_size_out) {
    *tile_size_out = tile_size;
  }
  if (gap_out) {
    *gap_out = gap;
  }
}

static void plumos_mali_graphic_draw_top_entries(
    struct plumos_mali_renderer *renderer,
    const struct plumos_mali_graphic_theme *theme,
    const struct plumos_mali_graphic_entry *entries, size_t entry_count,
    float x_offset, float y_offset) {
  size_t columns = 3;
  size_t rows = 2;
  float grid_x = 0.0f;
  float grid_y = 0.0f;
  float tile_size = 0.0f;
  float gap = 10.0f;
  float tile_w;
  float tile_h;
  size_t i;

  plumos_mali_graphic_top_layout_metrics(renderer, theme, &columns, &rows,
                                         &grid_x, &grid_y, &tile_size, &gap);
  tile_w = tile_size;
  tile_h = tile_size;
  (void)rows;

  for (i = 0; i < entry_count; i++) {
    size_t col = i % columns;
    size_t row = i / columns;
    float x = grid_x + x_offset + (float)col * (tile_w + gap);
    float y = grid_y + y_offset + (float)row * (tile_h + gap);
    float media_x = x + 14.0f;
    float media_y = y + 14.0f;
    float title_y = y + tile_h - 58.0f;
    float detail_y = y + tile_h - 30.0f;
    float media_w = tile_w - 28.0f;
    float media_h = title_y - media_y - 10.0f;
    int selected = entries[i].selected;
    char title[96];
    char detail[96];
    char initials[8];
    int initials_width;
    int detail_width;
    int logo_drawn = 0;

    plumos_mali_copy_utf8_cells(entries[i].title, title, sizeof(title), 18);
    plumos_mali_copy_utf8_cells(entries[i].detail, detail, sizeof(detail), 18);
    plumos_mali_graphic_initials(entries[i].title, initials, sizeof(initials));

    if (selected) {
      plumos_mali_graphic_rect(renderer, x, y, tile_w, tile_h,
                               theme->accent, 1.0f);
      plumos_mali_graphic_rect(renderer, x + 4.0f, y + 4.0f,
                               tile_w - 8.0f, tile_h - 8.0f,
                               theme->selection_background, 1.0f);
    } else {
      plumos_mali_graphic_rect(renderer, x, y, tile_w, tile_h,
                               theme->panel, 1.0f);
      plumos_mali_graphic_rect(renderer, x + 2.0f, y + 2.0f,
                               tile_w - 4.0f, tile_h - 4.0f,
                               theme->panel_inner, 1.0f);
    }
    plumos_mali_graphic_rect(renderer, media_x, media_y, media_w, media_h,
                             selected ? theme->selection_background
                                      : theme->media_panel,
                             1.0f);
#ifdef PLUMOS_ENABLE_MALI_PNG
    if (entries[i].thumbnail[0]) {
      logo_drawn =
          plumos_mali_graphic_draw_texture(renderer, entries[i].thumbnail,
                                           media_x + 4.0f, media_y + 4.0f,
                                           media_w - 8.0f, media_h - 8.0f);
    }
#endif
    if (!logo_drawn) {
      initials_width = plumos_mali_text_width(initials, 4);
      plumos_mali_graphic_text(renderer, initials,
                               x + (tile_w - (float)initials_width) * 0.5f,
                               media_y + (media_h - 32.0f) * 0.5f, 4,
                               selected ? theme->selection_foreground
                                        : theme->foreground,
                               1.0f);
    }
    plumos_mali_graphic_text_clipped(renderer, title, x + 12.0f, title_y,
                                     2, 1, x + 12.0f, x + tile_w - 12.0f,
                                     selected ? theme->selection_foreground
                                              : theme->foreground,
                                     1.0f);
    detail_width = plumos_mali_text_width(detail, 2);
    plumos_mali_graphic_text_clipped(renderer, detail,
                                     x + tile_w - 12.0f - (float)detail_width,
                                     detail_y, 2, 0,
                                     x + 12.0f, x + tile_w - 12.0f,
                                     theme->muted, 1.0f);
  }
  if (entry_count == 0) {
    plumos_mali_graphic_text(renderer, "NO SYSTEMS", 28.0f + x_offset,
                             90.0f + y_offset, 3, theme->foreground, 1.0f);
  }
}

static float plumos_mali_graphic_ease_slide_progress(float progress,
                                                     const char *easing) {
  if (progress < 0.0f) {
    progress = 0.0f;
  } else if (progress > 1.0f) {
    progress = 1.0f;
  }
  if (easing && strcmp(easing, "linear") == 0) {
    return progress;
  }
  return progress * progress * (3.0f - 2.0f * progress);
}

static void plumos_mali_graphic_draw_top(
    struct plumos_mali_renderer *renderer,
    const struct plumos_mali_graphic_theme *theme,
    const struct plumos_mali_graphic_entry *entries, size_t entry_count,
    const struct plumos_mali_graphic_entry *prev_entries, size_t prev_entry_count,
    const char *transition, int transition_direction, float transition_progress,
    const char *status) {
  float current_offset = 0.0f;
  float prev_offset = 0.0f;
  float current_x_offset = 0.0f;
  float current_y_offset = 0.0f;
  float prev_x_offset = 0.0f;
  float prev_y_offset = 0.0f;
  int slide_active = transition && strcmp(transition, "slide") == 0 &&
                     prev_entry_count > 0 && transition_progress < 1.0f;

  if (transition_progress < 0.0f) {
    transition_progress = 0.0f;
  } else if (transition_progress > 1.0f) {
    transition_progress = 1.0f;
  }

  if (slide_active) {
    int horizontal = strcmp(theme->transition_axis, "horizontal") == 0;
    float distance = horizontal ? (float)renderer->width : (float)renderer->height;
    float eased_progress =
        plumos_mali_graphic_ease_slide_progress(transition_progress,
                                                theme->transition_easing);
    int direction = transition_direction < 0 ? -1 : 1;
    prev_offset = -((float)direction) * distance * eased_progress;
    current_offset = ((float)direction) * distance * (1.0f - eased_progress);
    if (horizontal) {
      prev_x_offset = prev_offset;
      current_x_offset = current_offset;
    } else {
      prev_y_offset = prev_offset;
      current_y_offset = current_offset;
    }
    plumos_mali_graphic_draw_top_entries(renderer, theme, prev_entries,
                                         prev_entry_count, prev_x_offset,
                                         prev_y_offset);
  }

  plumos_mali_graphic_draw_top_entries(renderer, theme, entries, entry_count,
                                       current_x_offset, current_y_offset);
  plumos_mali_graphic_top_bar(renderer, theme, "PLUMOS A30 GUI");
  plumos_mali_graphic_rect(renderer, 0.0f, 0.0f, 5.0f, (float)renderer->height,
                           theme->accent, 1.0f);
  (void)status;
}

static void plumos_mali_graphic_draw_roms(
    struct plumos_mali_renderer *renderer,
    const struct plumos_mali_graphic_theme *theme,
    const char *mode, const char *system,
    const struct plumos_mali_graphic_entry *entries, size_t entry_count,
    const char *status) {
  size_t i;
  const struct plumos_mali_graphic_entry *selected = NULL;
  float list_x = 18.0f;
  float list_y = 70.0f;
  float list_w = 360.0f;
  float row_h = 38.0f;
  float preview_x = 394.0f;
  float preview_y = 72.0f;
  float preview_w = (float)renderer->width - preview_x - 16.0f;
  float preview_h = 310.0f;
  char header[96];
  char initials[8];
  int initials_width;
  int thumbnail_drawn = 0;

  plumos_mali_copy_utf8_cells(
      system && system[0] ? system :
          (mode && strcmp(mode, "favorites") == 0 ? "FAVORITES" :
           mode && strcmp(mode, "recent") == 0 ? "RECENT" : "ROMS"),
      header, sizeof(header), 24);
  plumos_mali_graphic_top_bar(renderer, theme, header);
  plumos_mali_graphic_rect(renderer, 0.0f, 0.0f, 5.0f, (float)renderer->height,
                           theme->accent, 1.0f);

  for (i = 0; i < entry_count; i++) {
    float y = list_y + (float)i * row_h;
    float cursor_y = y + 3.0f;
    float name_x = list_x + 24.0f;
    float name_right_x = list_x + list_w - 10.0f;
    int scroll_px = 0;
    struct plumos_mali_rgb text_color = theme->foreground;

    if (entries[i].selected || !selected) {
      selected = &entries[i];
    }
    if (entries[i].selected) {
      plumos_mali_graphic_rect(renderer, list_x - 6.0f, y - 7.0f, list_w,
                               row_h - 4.0f, theme->selection_background,
                               1.0f);
      if (name_right_x > name_x) {
        int title_width = plumos_mali_text_width_font(renderer, entries[i].title, 2, 1);
        int available_width = (int)(name_right_x - name_x);
        if (title_width > available_width) {
          scroll_px = plumos_mali_marquee_offset(renderer, title_width, available_width,
                                                 12);
        }
      }
      text_color = theme->selection_foreground;
    }
    plumos_mali_graphic_text(renderer, entries[i].selected ? ">" : " ",
                             list_x, cursor_y, 2, text_color, 1.0f);
    plumos_mali_graphic_text_clipped(renderer, entries[i].title,
                                     name_x - (float)scroll_px, y,
                                     2, 1, name_x, name_right_x,
                                     text_color, 1.0f);
  }

  plumos_mali_graphic_rect(renderer, preview_x, preview_y, preview_w,
                           preview_h, theme->panel, 1.0f);
  plumos_mali_graphic_rect(renderer, preview_x + 3.0f, preview_y + 3.0f,
                           preview_w - 6.0f, preview_h - 6.0f,
                           theme->panel_inner, 1.0f);
  plumos_mali_graphic_rect(renderer, preview_x + 16.0f, preview_y + 18.0f,
                           preview_w - 32.0f, 156.0f,
                           theme->media_panel, 1.0f);

  if (selected) {
#ifdef PLUMOS_ENABLE_MALI_PNG
    if (selected->thumbnail[0]) {
      thumbnail_drawn =
          plumos_mali_graphic_draw_texture(renderer, selected->thumbnail,
                                           preview_x + 16.0f, preview_y + 18.0f,
                                           preview_w - 32.0f, 156.0f);
    }
    if (!thumbnail_drawn && theme->placeholder_thumbnail[0]) {
      thumbnail_drawn =
          plumos_mali_graphic_draw_texture(renderer, theme->placeholder_thumbnail,
                                           preview_x + 16.0f, preview_y + 18.0f,
                                           preview_w - 32.0f, 156.0f);
    }
#endif
    plumos_mali_graphic_initials(selected->title, initials, sizeof(initials));
    if (!thumbnail_drawn) {
      initials_width = plumos_mali_text_width(initials, 4);
      plumos_mali_graphic_text(renderer, initials,
                               preview_x + (preview_w - (float)initials_width) * 0.5f,
                               preview_y + 76.0f, 4,
                               theme->foreground, 1.0f);
    }
    {
      const char *preview_title = selected->title;
      const char *preview_detail = selected->detail[0] ? selected->detail : "-";
      float preview_text_x = preview_x + 16.0f;
      float preview_text_right_x = preview_x + preview_w - 16.0f;
      int preview_available_width = (int)(preview_text_right_x - preview_text_x);
      int preview_title_scroll_px = 0;
      int preview_detail_scroll_px = 0;

      if (preview_available_width > 0) {
        int preview_title_width =
            plumos_mali_text_width_font(renderer, preview_title, 2, 1);
        int preview_detail_width =
            plumos_mali_text_width_font(renderer, preview_detail, 2, 1);
        if (preview_title_width > preview_available_width) {
          preview_title_scroll_px =
              plumos_mali_marquee_offset(renderer, preview_title_width,
                                         preview_available_width, 12);
        }
        if (preview_detail_width > preview_available_width) {
          preview_detail_scroll_px =
              plumos_mali_marquee_offset(renderer, preview_detail_width,
                                         preview_available_width, 12);
        }
      }
      plumos_mali_graphic_text_clipped(
          renderer, preview_title,
          preview_text_x - (float)preview_title_scroll_px,
          preview_y + 200.0f, 2, 1, preview_text_x, preview_text_right_x,
          theme->foreground, 1.0f);
      plumos_mali_graphic_text_clipped(
          renderer, preview_detail,
          preview_text_x - (float)preview_detail_scroll_px,
          preview_y + 232.0f, 2, 1, preview_text_x, preview_text_right_x,
          theme->muted, 1.0f);
    }
  } else {
    plumos_mali_graphic_text(renderer, "NO ART", preview_x + 48.0f,
                             preview_y + 120.0f, 3, theme->muted, 1.0f);
  }
  (void)status;
}

static size_t plumos_mali_graphic_selected_index(
    const struct plumos_mali_graphic_entry *entries, size_t entry_count) {
  size_t i;

  if (!entries || entry_count == 0) {
    return 0;
  }
  for (i = 0; i < entry_count; i++) {
    if (entries[i].selected) {
      return i;
    }
  }
  return 0;
}

static const struct plumos_mali_graphic_entry *plumos_mali_graphic_selected_entry(
    const struct plumos_mali_graphic_entry *entries, size_t entry_count) {
  if (!entries || entry_count == 0) {
    return NULL;
  }
  return &entries[plumos_mali_graphic_selected_index(entries, entry_count)];
}

static const struct plumos_mali_graphic_entry *
plumos_mali_graphic_selected_neighbor(
    const struct plumos_mali_graphic_entry *entries, size_t entry_count,
    int delta) {
  size_t selected_index;

  if (!entries || entry_count == 0 || delta == 0) {
    return NULL;
  }
  selected_index = plumos_mali_graphic_selected_index(entries, entry_count);
  if (delta < 0) {
    size_t step = (size_t)(-delta);
    if (selected_index < step) {
      return NULL;
    }
    return &entries[selected_index - step];
  }
  if (selected_index + (size_t)delta >= entry_count) {
    return NULL;
  }
  return &entries[selected_index + (size_t)delta];
}

static float plumos_mali_lerp_float(float from, float to, float progress) {
  return from + (to - from) * progress;
}

static void plumos_mali_graphic_draw_gallery_background(
    struct plumos_mali_renderer *renderer,
    const struct plumos_mali_graphic_theme *theme) {
  int row;
  const float wall_top = 40.0f;
  const float shelf_y = (float)renderer->height - 145.0f;

#ifdef PLUMOS_ENABLE_MALI_PNG
  if (theme->gallery_background_image[0] &&
      plumos_mali_graphic_draw_texture_cover(renderer,
                                             theme->gallery_background_image,
                                             0.0f, 0.0f,
                                             (float)renderer->width,
                                             (float)renderer->height)) {
    return;
  }
#endif

  plumos_mali_rect(renderer, 0.0f, 0.0f, (float)renderer->width,
                   (float)renderer->height, 0.035f, 0.020f, 0.014f, 1.0f);
  for (row = 0; row < 11; row++) {
    float y = wall_top + (float)row * 27.0f;
    float offset = (row % 2) ? 38.0f : 0.0f;
    float x;
    plumos_mali_rect(renderer, 0.0f, y, (float)renderer->width, 2.0f,
                     0.15f, 0.08f, 0.045f, 0.75f);
    for (x = -offset; x < (float)renderer->width; x += 76.0f) {
      plumos_mali_rect(renderer, x, y, 2.0f, 27.0f,
                       0.13f, 0.07f, 0.040f, 0.55f);
    }
  }
  plumos_mali_rect(renderer, 0.0f, shelf_y, (float)renderer->width, 20.0f,
                   0.35f, 0.18f, 0.08f, 1.0f);
  plumos_mali_rect(renderer, 0.0f, shelf_y + 20.0f, (float)renderer->width,
                   26.0f, 0.17f, 0.08f, 0.035f, 1.0f);
  plumos_mali_rect(renderer, 0.0f, shelf_y + 45.0f, (float)renderer->width,
                   (float)renderer->height - shelf_y - 45.0f,
                   0.03f, 0.025f, 0.020f, 1.0f);
}

static void plumos_mali_graphic_draw_gallery_card(
    struct plumos_mali_renderer *renderer,
    const struct plumos_mali_graphic_theme *theme,
    const struct plumos_mali_graphic_entry *entry,
    float x, float y, float w, float h, int selected) {
  char initials[8];
  int initials_width;
  int thumbnail_drawn = 0;

  if (!entry) {
    return;
  }
#ifdef PLUMOS_ENABLE_MALI_PNG
  if (entry->thumbnail[0]) {
    thumbnail_drawn =
        plumos_mali_graphic_draw_texture(renderer, entry->thumbnail,
                                         x, y, w, h);
  }
  if (!thumbnail_drawn && theme->placeholder_thumbnail[0]) {
    thumbnail_drawn =
        plumos_mali_graphic_draw_texture(renderer, theme->placeholder_thumbnail,
                                         x, y, w, h);
  }
#endif
  if (!thumbnail_drawn) {
    plumos_mali_graphic_initials(entry->title, initials, sizeof(initials));
    initials_width = plumos_mali_text_width(initials, selected ? 5 : 4);
    plumos_mali_graphic_text(renderer, initials,
                             x + (w - (float)initials_width) * 0.5f,
                             y + h * 0.45f, selected ? 5 : 4,
                             selected ? theme->selection_foreground
                                      : theme->foreground,
                             1.0f);
  }
}

static void plumos_mali_graphic_draw_gallery_entries(
    struct plumos_mali_renderer *renderer,
    const struct plumos_mali_graphic_theme *theme,
    const struct plumos_mali_graphic_entry *entries, size_t entry_count,
    float x_offset) {
  size_t selected_index;
  size_t i;
  const float center_w = 360.0f;
  const float center_h = 270.0f;
  const float side_w = 240.0f;
  const float side_h = 180.0f;
  const float center_x = ((float)renderer->width - center_w) * 0.5f;
  const float center_y = 84.0f;
  const float side_y = 128.0f;
  const float visible_side = 76.0f;

  if (!entries || entry_count == 0) {
    return;
  }
  selected_index = plumos_mali_graphic_selected_index(entries, entry_count);
  for (i = 0; i < entry_count; i++) {
    long rel = (long)i - (long)selected_index;
    if (rel < -1 || rel > 1) {
      continue;
    }
    if (rel == 0) {
      plumos_mali_graphic_draw_gallery_card(renderer, theme, &entries[i],
                                            center_x + x_offset, center_y,
                                            center_w, center_h, 1);
    } else if (rel < 0) {
      plumos_mali_graphic_draw_gallery_card(renderer, theme, &entries[i],
                                            -side_w + visible_side + x_offset,
                                            side_y, side_w, side_h, 0);
    } else {
      plumos_mali_graphic_draw_gallery_card(renderer, theme, &entries[i],
                                            (float)renderer->width -
                                                visible_side + x_offset,
                                            side_y, side_w, side_h, 0);
    }
  }
}

static void plumos_mali_graphic_draw_gallery_transition_entries(
    struct plumos_mali_renderer *renderer,
    const struct plumos_mali_graphic_theme *theme,
    const struct plumos_mali_graphic_entry *entries, size_t entry_count,
    const struct plumos_mali_graphic_entry *prev_entries,
    size_t prev_entry_count, int transition_direction,
    float transition_progress) {
  const struct plumos_mali_graphic_entry *old_entry =
      plumos_mali_graphic_selected_entry(prev_entries, prev_entry_count);
  const struct plumos_mali_graphic_entry *new_entry =
      plumos_mali_graphic_selected_entry(entries, entry_count);
  const struct plumos_mali_graphic_entry *edge_entry = NULL;
  const float center_w = 360.0f;
  const float center_h = 270.0f;
  const float side_w = 240.0f;
  const float side_h = 180.0f;
  const float center_x = ((float)renderer->width - center_w) * 0.5f;
  const float center_y = 84.0f;
  const float side_y = 128.0f;
  const float visible_side = 76.0f;
  const float left_x = -side_w + visible_side;
  const float right_x = (float)renderer->width - visible_side;
  float old_target_x;
  float new_start_x;

  if (!old_entry || !new_entry) {
    plumos_mali_graphic_draw_gallery_entries(renderer, theme, entries,
                                             entry_count, 0.0f);
    return;
  }
  if (transition_progress < 0.0f) {
    transition_progress = 0.0f;
  } else if (transition_progress > 1.0f) {
    transition_progress = 1.0f;
  }
  if (transition_direction < 0) {
    old_target_x = right_x;
    new_start_x = left_x;
    edge_entry =
        plumos_mali_graphic_selected_neighbor(entries, entry_count, -1);
  } else {
    old_target_x = left_x;
    new_start_x = right_x;
    edge_entry =
        plumos_mali_graphic_selected_neighbor(entries, entry_count, 1);
  }

  if (edge_entry) {
    plumos_mali_graphic_draw_gallery_card(
        renderer, theme, edge_entry,
        transition_direction < 0 ? left_x : right_x,
        side_y, side_w, side_h, 0);
  }
  plumos_mali_graphic_draw_gallery_card(
      renderer, theme, old_entry,
      plumos_mali_lerp_float(center_x, old_target_x, transition_progress),
      plumos_mali_lerp_float(center_y, side_y, transition_progress),
      plumos_mali_lerp_float(center_w, side_w, transition_progress),
      plumos_mali_lerp_float(center_h, side_h, transition_progress), 0);
  plumos_mali_graphic_draw_gallery_card(
      renderer, theme, new_entry,
      plumos_mali_lerp_float(new_start_x, center_x, transition_progress),
      plumos_mali_lerp_float(side_y, center_y, transition_progress),
      plumos_mali_lerp_float(side_w, center_w, transition_progress),
      plumos_mali_lerp_float(side_h, center_h, transition_progress), 1);
}

static void plumos_mali_graphic_draw_gallery_footer(
    struct plumos_mali_renderer *renderer,
    const struct plumos_mali_graphic_theme *theme,
    const struct plumos_mali_graphic_entry *selected) {
  const float footer_y = (float)renderer->height - 84.0f;
  const float text_left = 86.0f;
  const float text_right = (float)renderer->width - 86.0f;
  const char *title = selected && selected->title[0] ? selected->title : "NO ENTRY";
  int title_width = plumos_mali_text_width_font(renderer, title, 3, 1);
  int available_width = (int)(text_right - text_left);
  int scroll_px = 0;
  float title_x;

  plumos_mali_rect(renderer, 0.0f, footer_y, (float)renderer->width, 84.0f,
                   0.015f, 0.013f, 0.012f, 0.92f);
  plumos_mali_rect(renderer, 0.0f, footer_y, (float)renderer->width, 2.0f,
                   0.50f, 0.24f, 0.10f, 1.0f);
  if (title_width > available_width) {
    scroll_px = plumos_mali_marquee_offset(renderer, title_width,
                                           available_width, 12);
    title_x = text_left - (float)scroll_px;
  } else {
    title_x = ((float)renderer->width - (float)title_width) * 0.5f;
  }
  plumos_mali_graphic_text_clipped(renderer, title, title_x, footer_y + 23.0f,
                                   3, 1, text_left, text_right,
                                   theme->selection_foreground, 1.0f);
  plumos_mali_graphic_text(renderer, "<", 24.0f, footer_y + 27.0f, 3,
                           theme->muted, 1.0f);
  plumos_mali_graphic_text(renderer, ">",
                           (float)renderer->width - 42.0f, footer_y + 27.0f,
                           3, theme->muted, 1.0f);
}

static void plumos_mali_graphic_draw_gallery(
    struct plumos_mali_renderer *renderer,
    const struct plumos_mali_graphic_theme *theme,
    const char *system,
    const struct plumos_mali_graphic_entry *entries, size_t entry_count,
    const struct plumos_mali_graphic_entry *prev_entries, size_t prev_entry_count,
    const char *transition, int transition_direction, float transition_progress,
    const char *status) {
  size_t selected_index = plumos_mali_graphic_selected_index(entries, entry_count);
  const struct plumos_mali_graphic_entry *selected =
      entry_count > 0 ? &entries[selected_index] : NULL;
  int slide_active = transition && strcmp(transition, "slide") == 0 &&
                     prev_entry_count > 0 && transition_progress < 1.0f;

  plumos_mali_graphic_draw_gallery_background(renderer, theme);
  if (transition_progress < 0.0f) {
    transition_progress = 0.0f;
  } else if (transition_progress > 1.0f) {
    transition_progress = 1.0f;
  }
  if (slide_active) {
    float eased_progress =
        plumos_mali_graphic_ease_slide_progress(transition_progress,
                                                theme->transition_easing);
    plumos_mali_graphic_draw_gallery_transition_entries(
        renderer, theme, entries, entry_count, prev_entries, prev_entry_count,
        transition_direction < 0 ? -1 : 1, eased_progress);
  } else {
    plumos_mali_graphic_draw_gallery_entries(renderer, theme, entries,
                                             entry_count, 0.0f);
  }
  plumos_mali_graphic_top_bar(renderer, theme,
                              system && system[0] ? system : "GALLERY");
  plumos_mali_graphic_draw_gallery_footer(renderer, theme, selected);
  (void)status;
}

static int plumos_mali_render_lines_graphic(
    struct plumos_mali_renderer *renderer,
    const char lines[][PLUMOS_MALI_RENDER_LINE_MAX],
    size_t line_count) {
  size_t i;
  char mode[32];
  char system[128];
  char status[160];
  char transition[32];
  char transition_progress_text[32];
  char transition_direction_text[32];
  struct plumos_mali_graphic_entry entries[12];
  struct plumos_mali_graphic_entry prev_entries[12];
  struct plumos_mali_graphic_theme theme;
  size_t entry_count = 0;
  size_t prev_entry_count = 0;
  float transition_progress = 1.0f;
  int transition_direction = 1;

  plumos_mali_graphic_theme_load(lines, line_count, &theme);
  plumos_mali_find_prefixed_line(lines, line_count, "graphic_mode=",
                                 mode, sizeof(mode));
  plumos_mali_find_prefixed_line(lines, line_count, "graphic_system=",
                                 system, sizeof(system));
  plumos_mali_find_prefixed_line(lines, line_count, "status:",
                                 status, sizeof(status));
  plumos_mali_find_prefixed_line(lines, line_count, "graphic_transition=",
                                 transition, sizeof(transition));
  plumos_mali_find_prefixed_line(lines, line_count, "graphic_transition_progress=",
                                 transition_progress_text,
                                 sizeof(transition_progress_text));
  plumos_mali_find_prefixed_line(lines, line_count, "graphic_transition_direction=",
                                 transition_direction_text,
                                 sizeof(transition_direction_text));
  if (transition_progress_text[0]) {
    transition_progress = (float)strtod(transition_progress_text, NULL);
  }
  if (transition_direction_text[0]) {
    transition_direction = (int)strtol(transition_direction_text, NULL, 10);
  }

  for (i = 0; i < line_count && entry_count < sizeof(entries) / sizeof(entries[0]);
       i++) {
    if (plumos_mali_graphic_parse_entry(lines[i], &entries[entry_count])) {
      entry_count++;
    }
  }
  for (i = 0; i < line_count &&
              prev_entry_count < sizeof(prev_entries) / sizeof(prev_entries[0]);
       i++) {
    if (plumos_mali_graphic_parse_prev_entry(lines[i],
                                             &prev_entries[prev_entry_count])) {
      prev_entry_count++;
    }
  }

  renderer->gl.Viewport(0, 0, renderer->fb_width, renderer->fb_height);
  renderer->gl.UseProgram(renderer->program);
  renderer->gl.ClearColor(theme.background.r, theme.background.g,
                          theme.background.b, 1.0f);
  renderer->gl.Clear(GL_COLOR_BUFFER_BIT);
#ifdef PLUMOS_ENABLE_MALI_PNG
  if (theme.background_image[0]) {
    plumos_mali_graphic_draw_texture(renderer, theme.background_image,
                                     0.0f, 0.0f, (float)renderer->width,
                                     (float)renderer->height);
  }
#endif

  if (strcmp(mode, "top") == 0) {
    plumos_mali_graphic_draw_top(renderer, &theme, entries, entry_count,
                                 prev_entries, prev_entry_count, transition,
                                 transition_direction, transition_progress,
                                 status);
  } else if (strcmp(mode, "gallery") == 0) {
    plumos_mali_graphic_draw_gallery(renderer, &theme, system, entries,
                                     entry_count, prev_entries,
                                     prev_entry_count, transition,
                                     transition_direction,
                                     transition_progress, status);
  } else {
    plumos_mali_graphic_draw_roms(renderer, &theme, mode, system, entries,
                                  entry_count, status);
  }

  renderer->gl.Finish();
  return renderer->egl.SwapBuffers(renderer->display, renderer->surface) == EGL_TRUE;
}

static void plumos_mali_collect_lines(const char lines[][PLUMOS_MALI_RENDER_LINE_MAX],
                                      size_t line_count,
                                      char *title, size_t title_size,
                                      char *meta, size_t meta_size,
                                      char *status, size_t status_size,
                                      char entries[][PLUMOS_MALI_RENDER_LINE_MAX],
                                      size_t entry_capacity,
                                      size_t *entry_count_out) {
  size_t i;
  size_t entry_count = 0;

  if (title && title_size > 0) {
    title[0] = '\0';
  }
  if (meta && meta_size > 0) {
    meta[0] = '\0';
  }
  if (status && status_size > 0) {
    status[0] = '\0';
  }
  if (line_count > 0) {
    plumos_mali_make_title(lines[0], title, title_size);
  } else {
    plumos_mali_make_title(NULL, title, title_size);
  }
  for (i = 1; i < line_count; i++) {
    const char *line = lines[i];
    if (plumos_mali_is_empty_or_help(line)) {
      continue;
    }
    if (plumos_mali_starts_with(line, "status:")) {
      plumos_mali_make_meta(line, status, status_size);
      continue;
    }
    if (plumos_mali_starts_with(line, "prompt_path=")) {
      continue;
    }
    if (plumos_mali_starts_with(line, "usb_disk_starting=")) {
      continue;
    }
    if (plumos_mali_starts_with(line, "footer1=") ||
        plumos_mali_starts_with(line, "footer2=")) {
      continue;
    }
    if (plumos_mali_starts_with(line, "wifi_keyboard_cursor=") ||
        plumos_mali_starts_with(line, "wifi_password=")) {
      continue;
    }
    if (plumos_mali_starts_with(line, "source:")) {
      if (status && status_size > 0 && !status[0]) {
        plumos_mali_make_meta(line, status, status_size);
      }
      continue;
    }
    if (plumos_mali_starts_with(line, "entries=")) {
      continue;
    }
    if (plumos_mali_starts_with(line, "system=") ||
        plumos_mali_starts_with(line, "target=") || plumos_mali_starts_with(line, "profile=")) {
      if (meta && meta_size > 0 && !meta[0]) {
        plumos_mali_make_meta(line, meta, meta_size);
      }
      continue;
    }
    if (plumos_mali_is_entry_line(line) && entry_count < entry_capacity) {
      plumos_mali_make_entry(line, title, entries[entry_count], sizeof(entries[entry_count]));
      entry_count++;
    }
  }
  if (entry_count_out) {
    *entry_count_out = entry_count;
  }
}

static void plumos_mali_tty_top_bar(struct plumos_mali_renderer *renderer) {
  char time_label[16];
  char wifi_label[16];
  char battery_label[24];
  char right[64];
  int right_width;

  plumos_mali_time_label(time_label, sizeof(time_label));
  plumos_mali_wifi_label(wifi_label, sizeof(wifi_label));
  plumos_mali_battery_label(battery_label, sizeof(battery_label));
  snprintf(right, sizeof(right), "%s  %s  %s", time_label, wifi_label, battery_label);
  right_width = plumos_mali_text_width(right, 2);

  plumos_mali_rect(renderer, 0.0f, 0.0f, (float)renderer->width, 34.0f,
                   0.000f, 0.020f, 0.015f, 1.0f);
  plumos_mali_rect(renderer, 0.0f, 34.0f, (float)renderer->width, 2.0f,
                   0.12f, 0.32f, 0.22f, 1.0f);
  plumos_mali_text(renderer, "PLUMOS A30 TTY1", 14.0f, 10.0f, 2,
                   0.62f, 1.0f, 0.78f, 1.0f);
  plumos_mali_text(renderer, right, (float)(renderer->width - 14 - right_width),
                   10.0f, 2, 0.70f, 0.92f, 0.86f, 1.0f);
}

static int plumos_mali_tty_entry_parts(const char *line, int *selected,
                                       char *number, size_t number_size,
                                       char *name, size_t name_size,
                                       char *count, size_t count_size) {
  char marker;
  char raw_number[16];
  const char *rest;
  const char *roms;
  size_t len;

  if (selected) {
    *selected = 0;
  }
  if (number && number_size > 0) {
    number[0] = '\0';
  }
  if (name && name_size > 0) {
    name[0] = '\0';
  }
  if (count && count_size > 0) {
    count[0] = '\0';
  }
  if (!plumos_mali_entry_head(line, &marker, raw_number, sizeof(raw_number), &rest)) {
    return 0;
  }
  if (selected) {
    *selected = marker == '>';
  }
  if (number && number_size > 0) {
    if (strlen(raw_number) == 1) {
      snprintf(number, number_size, "0%s", raw_number);
    } else {
      snprintf(number, number_size, "%s", raw_number);
    }
  }

  roms = strstr(rest, " ROMS=");
  if (!roms) {
    roms = strstr(rest, " ROMs=");
  }
  if (roms) {
    len = (size_t)(roms - rest);
    if (name && name_size > 0) {
      if (len >= name_size) {
        len = name_size - 1;
      }
      memcpy(name, rest, len);
      name[len] = '\0';
      plumos_mali_trim_right(name);
    }
    if (count && count_size > 0) {
      snprintf(count, count_size, "%s ROMS", roms + 6);
      plumos_mali_truncate(count, 9);
    }
  } else {
    if (name && name_size > 0) {
      plumos_mali_copy_utf8_cells(rest, name, name_size, 240);
    }
  }
  return 1;
}

static int plumos_mali_tty_font_scale(const struct plumos_mali_renderer *renderer) {
  if (renderer && renderer->tty_entry_scale_x10 >= 20) {
    return 4;
  }
  if (renderer && renderer->tty_entry_scale_x10 >= 15) {
    return 3;
  }
  return 2;
}

static size_t plumos_mali_tty_name_cells(int entry_scale) {
  if (entry_scale >= 4) {
    return 14;
  }
  if (entry_scale == 3) {
    return 18;
  }
  return 24;
}

static int plumos_mali_split_setting_control(const char *in,
                                             char *label, size_t label_size,
                                             char *control, size_t control_size,
                                             int *flash_direction) {
  const char *choice;
  char *marker;
  size_t label_len;

  if (flash_direction) {
    *flash_direction = 0;
  }
  if (!in || !label || label_size == 0 || !control || control_size == 0) {
    return 0;
  }
  label[0] = '\0';
  control[0] = '\0';
  if ((strncmp(in, "[x] ", 4) == 0 || strncmp(in, "[ ] ", 4) == 0) && in[4]) {
    snprintf(control, control_size, "%.3s", in);
    snprintf(label, label_size, "%s", in + 4);
    plumos_mali_trim_right(label);
    return label[0] != '\0';
  }

  choice = strstr(in, " < ");
  if (!choice || !strstr(choice + 1, " >")) {
    return 0;
  }
  label_len = (size_t)(choice - in);
  if (label_len >= label_size) {
    label_len = label_size - 1;
  }
  memcpy(label, in, label_len);
  label[label_len] = '\0';
  plumos_mali_trim_right(label);
  snprintf(control, control_size, "%s", choice + 1);
  marker = strstr(control, PLUMOS_MALI_SETTING_FLASH_MARKER);
  if (marker) {
    char direction = marker[strlen(PLUMOS_MALI_SETTING_FLASH_MARKER)];
    if (flash_direction && direction == 'L') {
      *flash_direction = -1;
    } else if (flash_direction && direction == 'R') {
      *flash_direction = 1;
    }
    *marker = '\0';
  }
  plumos_mali_trim_right(control);
  return label[0] != '\0' && control[0] != '\0';
}

static void plumos_mali_text_setting_control(struct plumos_mali_renderer *renderer,
                                             const char *control, float x, float y,
                                             int scale, int flash_direction,
                                             float red, float green, float blue,
                                             float alpha) {
  const char *arrow;
  char before[80];
  char after[80];
  size_t before_len;
  int before_width;
  int arrow_width;

  if (!control || flash_direction == 0) {
    plumos_mali_text(renderer, control, x, y, scale, red, green, blue, alpha);
    return;
  }

  arrow = flash_direction < 0 ? strchr(control, '<') : strrchr(control, '>');
  if (!arrow) {
    plumos_mali_text(renderer, control, x, y, scale, red, green, blue, alpha);
    return;
  }

  before_len = (size_t)(arrow - control);
  if (before_len >= sizeof(before)) {
    before_len = sizeof(before) - 1;
  }
  memcpy(before, control, before_len);
  before[before_len] = '\0';
  snprintf(after, sizeof(after), "%s", arrow + 1);

  before_width = plumos_mali_text_width(before, scale);
  arrow_width = plumos_mali_text_width("<", scale);
  plumos_mali_text(renderer, before, x, y, scale, red, green, blue, alpha);
  plumos_mali_text(renderer, flash_direction < 0 ? "<" : ">",
                   x + (float)before_width, y, scale,
                   1.0f, 0.08f, 0.04f, alpha);
  plumos_mali_text(renderer, after, x + (float)(before_width + arrow_width), y,
                   scale, red, green, blue, alpha);
}

static void plumos_mali_text_token_row(struct plumos_mali_renderer *renderer,
                                       const char *text, int selected_token,
                                       float x, float y, int scale, float max_x,
                                       float red, float green, float blue,
                                       float alpha) {
  const char *p = text;
  float pen_x = 0.0f;
  int token_index = 0;
  int space_width = plumos_mali_text_width(" ", scale);

  if (!renderer || !text || scale <= 0) {
    return;
  }
  while (*p) {
    char token[32];
    size_t token_len = 0;
    int token_width;
    int is_selected;

    while (*p == ' ') {
      pen_x += (float)space_width;
      p++;
    }
    if (!*p || x + pen_x >= max_x) {
      break;
    }
    while (p[token_len] && p[token_len] != ' ' && token_len + 1 < sizeof(token)) {
      token[token_len] = p[token_len];
      token_len++;
    }
    token[token_len] = '\0';
    while (p[token_len] && p[token_len] != ' ') {
      token_len++;
    }
    p += token_len;
    if (!token[0]) {
      continue;
    }

    token_width = plumos_mali_text_width(token, scale);
    is_selected = token_index == selected_token;
    if (is_selected) {
      plumos_mali_rect(renderer, x + pen_x - 3.0f, y - 4.0f,
                       (float)token_width + 6.0f, (float)(scale * 7 + 8),
                       0.22f, 0.02f, 0.02f, 1.0f);
      plumos_mali_text(renderer, token, x + pen_x, y, scale,
                       1.0f, 0.08f, 0.04f, alpha);
    } else {
      plumos_mali_text(renderer, token, x + pen_x, y, scale,
                       red, green, blue, alpha);
    }
    pen_x += (float)token_width;
    token_index++;
  }
}

static int plumos_mali_title_is_rom_list(const char *title) {
  return title && (strstr(title, "ROMS") || strstr(title, "FAVORITES") ||
                   strstr(title, "RECENT"));
}

static int plumos_mali_title_is_top(const char *title) {
  return title && strstr(title, "TOP");
}

static int plumos_mali_title_is_settings(const char *title) {
  return title && strstr(title, "SETTINGS");
}

static int plumos_mali_title_is_settings_family(const char *title) {
  return title && (strstr(title, "START") || strstr(title, "Apps") ||
                   strstr(title, "APPS") || strstr(title, "Settings") ||
                   strstr(title, "SETTINGS") || strstr(title, "HELP") ||
                   strstr(title, "Thumbnail Results") ||
                   strstr(title, "Scraping") ||
                   strstr(title, "Network") || strstr(title, "NETWORK"));
}

static int plumos_mali_title_is_brightness_test(const char *title) {
  return title && strstr(title, "Brightness Test");
}

static int plumos_mali_render_lines_tty(struct plumos_mali_renderer *renderer,
                                        const char lines[][PLUMOS_MALI_RENDER_LINE_MAX],
                                        size_t line_count) {
  size_t i;
  char prompt[PLUMOS_MALI_RENDER_LINE_MAX + 64];
  char prompt_path[PLUMOS_MALI_RENDER_LINE_MAX];
  char prompt_line1[160];
  char prompt_line2[160];
  char title[80];
  char meta[160];
  char status[160];
  char footer1[160];
  char footer2[160];
  char wifi_password[160];
  char thumbnail_running_title[160];
  char thumbnail_running_phase[64];
  char thumbnail_running_system[64];
  char thumbnail_running_progress[64];
  char thumbnail_running_stats[64];
  char entries[18][PLUMOS_MALI_RENDER_LINE_MAX];
  size_t entry_count = 0;
  float y;
  int entry_scale;
  float line_height;
  float cursor_x;
  float name_x;
  float count_right_x;
  float right_margin;
  int cell_width;
  int name_col;
  int prompt_cursor_width;
  float prompt_cursor_y;
  size_t prompt_cells_per_line;
  size_t prompt_line1_len;
  size_t prompt_line2_len;
  size_t prompt_len;
  int is_rom_list;
  int is_top;
  int is_settings;
  int is_settings_family;
  int is_settings_page;
  int is_brightness_test;
  int show_prompt;
  int is_usb_disk_starting;
  int is_thumbnail_running;
  int wifi_keyboard_row = -1;
  int wifi_keyboard_col = -1;
  int brightness_tile_values[24];
  int brightness_tile_selected[24];
  int brightness_tile_current[24];
  size_t brightness_tile_count = 0;
  float prompt_r = 0.62f;
  float prompt_g = 1.00f;
  float prompt_b = 0.78f;
  float accent_r = 0.0f;
  float accent_g = 0.0f;
  float accent_b = 0.0f;
  int show_accent = 0;

  plumos_mali_collect_lines(lines, line_count, title, sizeof(title),
                            meta, sizeof(meta), status, sizeof(status),
                            entries, sizeof(entries) / sizeof(entries[0]), &entry_count);
  plumos_mali_find_prompt_path(lines, line_count, prompt_path, sizeof(prompt_path));
  plumos_mali_find_footer_lines(lines, line_count, footer1, sizeof(footer1),
                                footer2, sizeof(footer2));
  plumos_mali_find_wifi_keyboard_cursor(lines, line_count,
                                        &wifi_keyboard_row, &wifi_keyboard_col);
  plumos_mali_find_prefixed_line(lines, line_count, "wifi_password=",
                                 wifi_password, sizeof(wifi_password));
  plumos_mali_find_prefixed_line(lines, line_count, "thumbnail_running_title=",
                                 thumbnail_running_title,
                                 sizeof(thumbnail_running_title));
  plumos_mali_find_prefixed_line(lines, line_count, "thumbnail_running_phase=",
                                 thumbnail_running_phase,
                                 sizeof(thumbnail_running_phase));
  plumos_mali_find_prefixed_line(lines, line_count, "thumbnail_running_system=",
                                 thumbnail_running_system,
                                 sizeof(thumbnail_running_system));
  plumos_mali_find_prefixed_line(lines, line_count, "thumbnail_running_progress=",
                                 thumbnail_running_progress,
                                 sizeof(thumbnail_running_progress));
  plumos_mali_find_prefixed_line(lines, line_count, "thumbnail_running_stats=",
                                 thumbnail_running_stats,
                                 sizeof(thumbnail_running_stats));
  is_usb_disk_starting = plumos_mali_has_prefixed_line(lines, line_count,
                                                       "usb_disk_starting=1");
  is_thumbnail_running = plumos_mali_has_prefixed_line(lines, line_count,
                                                       "thumbnail_running=1") ||
                         (title[0] && strstr(title, "Thumbnail Running") != NULL);
  if (is_usb_disk_starting) {
    const char *line1 = "USB DISK MODE";
    const char *line2 = "STARTING";
    const char *line3 = "PLEASE WAIT";
    const int scale1 = 4;
    const int scale2 = 4;
    const int scale3 = 3;
    float y1 = (float)renderer->height * 0.5f - 88.0f;
    float y2 = y1 + 58.0f;
    float y3 = y2 + 76.0f;
    float x1 = ((float)renderer->width - (float)plumos_mali_text_width(line1, scale1)) * 0.5f;
    float x2 = ((float)renderer->width - (float)plumos_mali_text_width(line2, scale2)) * 0.5f;
    float x3 = ((float)renderer->width - (float)plumos_mali_text_width(line3, scale3)) * 0.5f;

    renderer->gl.Viewport(0, 0, renderer->fb_width, renderer->fb_height);
    renderer->gl.UseProgram(renderer->program);
    renderer->gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    renderer->gl.Clear(GL_COLOR_BUFFER_BIT);
    plumos_mali_text(renderer, line1, x1, y1, scale1, 1.0f, 0.04f, 0.02f, 1.0f);
    plumos_mali_text(renderer, line2, x2, y2, scale2, 1.0f, 0.04f, 0.02f, 1.0f);
    plumos_mali_text(renderer, line3, x3, y3, scale3, 1.0f, 0.04f, 0.02f, 1.0f);
    renderer->gl.Finish();
    return renderer->egl.SwapBuffers(renderer->display, renderer->surface) == EGL_TRUE;
  }
  if (is_thumbnail_running) {
    const char *line1 = strstr(thumbnail_running_title, "Scraping") ? "SCRAPING" : "THUMBNAIL";
    const char *line2 = "RUNNING";
    char line3[80];
    char line4[96];
    char line5[96];
    const int scale1 = 4;
    const int scale2 = 4;
    const int scale3 = 2;
    const int scale4 = 2;
    const int scale5 = 2;
    float y1 = (float)renderer->height * 0.5f - 132.0f;
    float y2 = y1 + 58.0f;
    float y3 = y2 + 66.0f;
    float y4 = y3 + 34.0f;
    float y5 = y4 + 30.0f;
    float x1 = ((float)renderer->width - (float)plumos_mali_text_width(line1, scale1)) * 0.5f;
    float x2 = ((float)renderer->width - (float)plumos_mali_text_width(line2, scale2)) * 0.5f;
    float x3;
    float x4;
    float x5;

    if (thumbnail_running_progress[0] &&
        strcmp(thumbnail_running_progress, "0/0") != 0) {
      snprintf(line3, sizeof(line3), "PROGRESS %s", thumbnail_running_progress);
    } else {
      snprintf(line3, sizeof(line3), "%.70s",
               thumbnail_running_title[0] ? thumbnail_running_title : "PLEASE WAIT");
    }
    if (thumbnail_running_phase[0]) {
      snprintf(line4, sizeof(line4), "%.30s %.40s", thumbnail_running_phase,
               thumbnail_running_system[0] ? thumbnail_running_system : "-");
    } else {
      snprintf(line4, sizeof(line4), "PLEASE WAIT");
    }
    if (thumbnail_running_stats[0]) {
      snprintf(line5, sizeof(line5), "SAVED/NOMATCH/FAIL %s", thumbnail_running_stats);
    } else {
      snprintf(line5, sizeof(line5), "RESULTS OPEN WHEN DONE");
    }
    x3 = ((float)renderer->width - (float)plumos_mali_text_width(line3, scale3)) * 0.5f;
    x4 = ((float)renderer->width - (float)plumos_mali_text_width(line4, scale4)) * 0.5f;
    x5 = ((float)renderer->width - (float)plumos_mali_text_width(line5, scale5)) * 0.5f;

    renderer->gl.Viewport(0, 0, renderer->fb_width, renderer->fb_height);
    renderer->gl.UseProgram(renderer->program);
    renderer->gl.ClearColor(0.0f, 0.006f, 0.005f, 1.0f);
    renderer->gl.Clear(GL_COLOR_BUFFER_BIT);
    plumos_mali_tty_top_bar(renderer);
    plumos_mali_rect(renderer, 0.0f, 0.0f, 7.0f, (float)renderer->height,
                     1.0f, 0.52f, 0.05f, 1.0f);
    plumos_mali_text(renderer, line1, x1, y1, scale1,
                     1.0f, 0.52f, 0.05f, 1.0f);
    plumos_mali_text(renderer, line2, x2, y2, scale2,
                     1.0f, 0.86f, 0.28f, 1.0f);
    plumos_mali_text(renderer, line3, x3 < 14.0f ? 14.0f : x3, y3, scale3,
                     0.78f, 0.94f, 0.90f, 1.0f);
    plumos_mali_text(renderer, line4, x4 < 14.0f ? 14.0f : x4, y4, scale4,
                     0.22f, 0.58f, 1.0f, 1.0f);
    plumos_mali_text(renderer, line5, x5 < 14.0f ? 14.0f : x5, y5, scale5,
                     1.0f, 0.86f, 0.28f, 1.0f);
    renderer->gl.Finish();
    return renderer->egl.SwapBuffers(renderer->display, renderer->surface) == EGL_TRUE;
  }
  is_rom_list = plumos_mali_title_is_rom_list(title);
  is_top = plumos_mali_title_is_top(title);
  is_settings = plumos_mali_title_is_settings(title);
  is_settings_family = plumos_mali_title_is_settings_family(title);
  is_settings_page = strstr(title, "Settings") != NULL;
  is_brightness_test = plumos_mali_title_is_brightness_test(title);
  show_prompt = !is_settings_family;
  if (is_brightness_test) {
    const char *prefix = "brightness_tile=";
    size_t prefix_len = strlen(prefix);
    for (i = 0; i < line_count && brightness_tile_count <
                    sizeof(brightness_tile_values) / sizeof(brightness_tile_values[0]);
         i++) {
      const char *line = lines[i];
      char *endptr = NULL;
      long value;
      if (!plumos_mali_starts_with(line, prefix)) {
        continue;
      }
      value = strtol(line + prefix_len, &endptr, 10);
      if (endptr == line + prefix_len) {
        continue;
      }
      brightness_tile_values[brightness_tile_count] = (int)value;
      brightness_tile_selected[brightness_tile_count] =
          strstr(endptr, "selected=1") != NULL;
      brightness_tile_current[brightness_tile_count] =
          strstr(endptr, "current=1") != NULL;
      brightness_tile_count++;
    }
  }
  snprintf(prompt, sizeof(prompt), "root@PlumOS A30:~# ls -n -c ./systems/top");
  if (is_rom_list && prompt_path[0]) {
    snprintf(prompt, sizeof(prompt), "root@PlumOS A30:~# ls -n -c %s", prompt_path);
  }
  if (is_top || is_rom_list) {
    prompt_r = 0.92f;
    prompt_g = 0.96f;
    prompt_b = 0.94f;
  }
  if (is_top || is_rom_list) {
    accent_r = 1.0f;
    accent_g = 0.52f;
    accent_b = 0.05f;
    show_accent = 1;
  } else if (is_settings_family || is_settings) {
    accent_r = 0.22f;
    accent_g = 0.58f;
    accent_b = 1.0f;
    show_accent = 1;
  }
  entry_scale = plumos_mali_tty_font_scale(renderer);
  if (is_settings_family) {
    entry_scale = 3;
  }
  line_height = (float)(entry_scale * 12);
  cell_width = 6 * entry_scale;
  name_col = 2;
  cursor_x = 18.0f;
  right_margin = 18.0f;
  if (is_settings_family) {
    cursor_x = 12.0f;
    name_col = 1;
    right_margin = 8.0f;
  }
  name_x = cursor_x + (float)(name_col * cell_width);
  count_right_x = (float)renderer->width - right_margin;
  prompt_len = strlen(prompt);
  prompt_cells_per_line = (size_t)(((float)renderer->width - 22.0f) / 12.0f);
  if (prompt_cells_per_line >= sizeof(prompt_line1)) {
    prompt_cells_per_line = sizeof(prompt_line1) - 1;
  }
  prompt_line1_len = prompt_len < prompt_cells_per_line ? prompt_len : prompt_cells_per_line;
  prompt_line2_len = prompt_len - prompt_line1_len;
  if (prompt_line2_len >= sizeof(prompt_line2)) {
    prompt_line2_len = sizeof(prompt_line2) - 1;
  }
  memcpy(prompt_line1, prompt, prompt_line1_len);
  prompt_line1[prompt_line1_len] = '\0';
  memcpy(prompt_line2, prompt + prompt_line1_len, prompt_line2_len);
  prompt_line2[prompt_line2_len] = '\0';

  renderer->gl.Viewport(0, 0, renderer->fb_width, renderer->fb_height);
  renderer->gl.UseProgram(renderer->program);
  renderer->gl.ClearColor(0.000f, 0.006f, 0.005f, 1.0f);
  renderer->gl.Clear(GL_COLOR_BUFFER_BIT);

  plumos_mali_tty_top_bar(renderer);
  if (show_accent) {
    plumos_mali_rect(renderer, 0.0f, 0.0f, 5.0f, (float)renderer->height,
                     accent_r, accent_g, accent_b, 1.0f);
  }
  if (show_prompt) {
    plumos_mali_text(renderer, prompt_line1, 14.0f, 48.0f, 2,
                     prompt_r, prompt_g, prompt_b, 1.0f);
    if (prompt_line2[0]) {
      plumos_mali_text(renderer, prompt_line2, 14.0f, 66.0f, 2,
                       prompt_r, prompt_g, prompt_b, 1.0f);
      prompt_cursor_width = plumos_mali_text_width(prompt_line2, 2);
      prompt_cursor_y = 66.0f;
    } else {
      prompt_cursor_width = plumos_mali_text_width(prompt_line1, 2);
      prompt_cursor_y = 48.0f;
    }
    if ((time(NULL) & 1) == 0 &&
        14.0f + (float)prompt_cursor_width + 12.0f < (float)renderer->width - 8.0f) {
      plumos_mali_rect(renderer, 14.0f + (float)prompt_cursor_width + 12.0f,
                       prompt_cursor_y, 10.0f, 14.0f,
                       prompt_r, prompt_g, prompt_b, 1.0f);
    }
    y = 104.0f;
  } else {
    plumos_mali_text(renderer, title, 14.0f, 48.0f, 2,
                     0.72f, 0.88f, 1.0f, 1.0f);
    y = 82.0f;
  }

  if (is_brightness_test) {
    const size_t columns = 4;
    const float grid_x = 18.0f;
    const float grid_y = 92.0f;
    const float gap = 10.0f;
    const float tile_w = ((float)renderer->width - grid_x * 2.0f -
                          gap * (float)(columns - 1)) /
                         (float)columns;
    const float tile_h = 54.0f;
    size_t tile;

    for (tile = 0; tile < brightness_tile_count; tile++) {
      size_t col = tile % columns;
      size_t row = tile / columns;
      float x = grid_x + (float)col * (tile_w + gap);
      float tile_y = grid_y + (float)row * (tile_h + gap);
      int selected_tile = brightness_tile_selected[tile];
      int current_tile = brightness_tile_current[tile];
      char label[16];
      int label_width;
      float label_x;
      float label_y;

      snprintf(label, sizeof(label), "%d", brightness_tile_values[tile]);
      if (selected_tile) {
        plumos_mali_rect(renderer, x, tile_y, tile_w, tile_h,
                         1.0f, 0.76f, 0.20f, 1.0f);
        plumos_mali_rect(renderer, x + 3.0f, tile_y + 3.0f,
                         tile_w - 6.0f, tile_h - 6.0f,
                         0.10f, 0.25f, 0.20f, 1.0f);
      } else {
        plumos_mali_rect(renderer, x, tile_y, tile_w, tile_h,
                         0.05f, 0.13f, 0.15f, 1.0f);
        plumos_mali_rect(renderer, x + 2.0f, tile_y + 2.0f,
                         tile_w - 4.0f, tile_h - 4.0f,
                         0.00f, 0.03f, 0.03f, 1.0f);
      }
      if (current_tile && !selected_tile) {
        plumos_mali_rect(renderer, x + 4.0f, tile_y + tile_h - 8.0f,
                         tile_w - 8.0f, 4.0f,
                         0.22f, 0.58f, 1.0f, 1.0f);
      }

      label_width = plumos_mali_text_width(label, 3);
      label_x = x + (tile_w - (float)label_width) / 2.0f;
      label_y = tile_y + 16.0f;
      plumos_mali_text(renderer, label, label_x, label_y, 3,
                       selected_tile ? 1.0f : 0.72f,
                       selected_tile ? 0.88f : 0.82f,
                       selected_tile ? 0.40f : 0.78f,
                       1.0f);
      if (current_tile) {
        const char *current_label = "CURRENT";
        int current_width = plumos_mali_text_width(current_label, 2);
        plumos_mali_text(renderer, current_label,
                         x + (tile_w - (float)current_width) / 2.0f,
                         tile_y + tile_h - 22.0f, 2,
                         0.64f, 0.82f, 0.92f, 1.0f);
      }
    }

    if (footer1[0] || footer2[0]) {
      plumos_mali_rect(renderer, 0.0f, (float)renderer->height - 74.0f,
                       (float)renderer->width, 74.0f,
                       0.000f, 0.018f, 0.020f, 1.0f);
      plumos_mali_rect(renderer, 0.0f, (float)renderer->height - 76.0f,
                       (float)renderer->width, 2.0f,
                       0.06f, 0.18f, 0.25f, 1.0f);
      if (footer1[0]) {
        plumos_mali_text(renderer, footer1, 14.0f,
                         (float)renderer->height - 56.0f, 2,
                         0.64f, 0.82f, 0.92f, 1.0f);
      }
      if (footer2[0]) {
        plumos_mali_text(renderer, footer2, 14.0f,
                         (float)renderer->height - 34.0f, 2,
                         0.64f, 0.82f, 0.92f, 1.0f);
      }
    }
    (void)status;
    renderer->gl.Finish();
    return renderer->egl.SwapBuffers(renderer->display, renderer->surface) == EGL_TRUE;
  }

  for (i = 0; i < entry_count; i++) {
    int selected = 0;
    int keyboard_selected = 0;
    char number[24];
    char name[PLUMOS_MALI_RENDER_LINE_MAX];
    char rom_name[PLUMOS_MALI_RENDER_LINE_MAX];
    char visible_name[PLUMOS_MALI_RENDER_LINE_MAX];
    char count[32];
    float count_x;
    float name_right_x;
    int name_width;
    int scroll_px;
    const char *draw_name;
    size_t name_cells;
    float cursor_y;
    float highlight_y;
    float highlight_h;
    float r = 0.72f;
    float g = 0.82f;
    float b = 0.78f;
    if (!plumos_mali_tty_entry_parts(entries[i], &selected, number, sizeof(number),
                                     name, sizeof(name), count, sizeof(count))) {
      continue;
    }
    if (wifi_keyboard_row >= 0 && wifi_keyboard_col >= 0 &&
        atoi(number) == wifi_keyboard_row + 1) {
      keyboard_selected = 1;
    }
    count_x = count[0] ? count_right_x - (float)plumos_mali_text_width(count, entry_scale)
                       : count_right_x;
    if (count_x < name_x + (float)(3 * cell_width)) {
      count_x = name_x + (float)(3 * cell_width);
    }
    name_right_x = count[0] ? count_x - (float)cell_width : count_right_x;
    if (name_right_x > (float)renderer->width - right_margin) {
      name_right_x = (float)renderer->width - right_margin;
    }
    if (count[0]) {
      name_cells = (size_t)((count_x - name_x - (float)cell_width) / (float)cell_width);
    } else {
      name_cells = (size_t)((name_right_x - name_x) / (float)cell_width);
    }
    if (!is_rom_list && !is_settings_family &&
        name_cells > plumos_mali_tty_name_cells(entry_scale)) {
      name_cells = plumos_mali_tty_name_cells(entry_scale);
    }
    if (name_cells < 1) {
      name_cells = 1;
    }
    if (is_rom_list) {
      plumos_mali_normalize_kana_marks(name, rom_name, sizeof(rom_name));
    } else {
      plumos_mali_copy_utf8_cells(name, visible_name, sizeof(visible_name), name_cells);
      if (is_top) {
        plumos_mali_ascii_upper_inplace(visible_name);
      }
    }
    if (selected && !keyboard_selected) {
      highlight_y = y - 7.0f;
      highlight_h = (float)(entry_scale * 7 + 10);
      if (is_rom_list) {
        highlight_y = y + (float)(entry_scale / 2);
        highlight_h = (float)(entry_scale * 7 + 8);
      }
      plumos_mali_rect(renderer, 10.0f, highlight_y, (float)renderer->width - 20.0f,
                       highlight_h, 0.12f, 0.23f, 0.17f, 1.0f);
      r = 1.0f;
      g = 0.88f;
      b = 0.40f;
    }
    cursor_y = y;
    if (is_rom_list) {
      cursor_y += (float)((entry_scale * 3) / 2);
    }
    plumos_mali_text(renderer, selected ? ">" : " ", cursor_x, cursor_y,
                     entry_scale, r, g, b, 1.0f);
    (void)number;
	    if (is_rom_list) {
	      draw_name = rom_name;
	      scroll_px = 0;
	      if (selected) {
	        name_width = plumos_mali_text_width_font(renderer, rom_name, entry_scale, 1);
        if (name_width > (int)(name_right_x - name_x)) {
          scroll_px = plumos_mali_marquee_offset(renderer, name_width,
                                                 (int)(name_right_x - name_x),
                                                 cell_width / 2);
        }
	      }
	      plumos_mali_text_clipped(renderer, draw_name, name_x - (float)scroll_px, y,
	                               entry_scale, 1, name_x, name_right_x, r, g, b, 1.0f);
		    } else if (wifi_keyboard_row >= 0) {
		      plumos_mali_text_token_row(renderer, name,
		                                 keyboard_selected ? wifi_keyboard_col : -1,
		                                 name_x, y,
		                                 entry_scale, name_right_x, r, g, b, 1.0f);
		    } else if (is_settings_page) {
	      char setting_label[PLUMOS_MALI_RENDER_LINE_MAX];
		      char setting_control[80];
		      char visible_label[PLUMOS_MALI_RENDER_LINE_MAX];
		      int control_width;
		      float control_x;
		      size_t label_cells;
		      int flash_direction = 0;

		      if (plumos_mali_split_setting_control(name, setting_label, sizeof(setting_label),
		                                            setting_control, sizeof(setting_control),
		                                            &flash_direction)) {
	        control_width = plumos_mali_text_width(setting_control, entry_scale);
	        control_x = count_right_x - (float)control_width;
	        if (control_x < name_x + (float)(6 * cell_width)) {
	          control_x = name_x + (float)(6 * cell_width);
	        }
	        label_cells = (size_t)((control_x - name_x - (float)cell_width) /
	                               (float)cell_width);
	        if (label_cells < 1) {
	          label_cells = 1;
	        }
		        plumos_mali_copy_utf8_cells(setting_label, visible_label, sizeof(visible_label),
		                                    label_cells);
		        plumos_mali_text(renderer, visible_label, name_x, y, entry_scale, r, g, b, 1.0f);
		        plumos_mali_text_setting_control(renderer, setting_control, control_x, y,
		                                         entry_scale, flash_direction, r, g, b,
		                                         1.0f);
		      } else {
	        plumos_mali_text(renderer, visible_name, name_x, y, entry_scale, r, g, b, 1.0f);
	      }
	    } else {
	      plumos_mali_text(renderer, visible_name, name_x, y, entry_scale, r, g, b, 1.0f);
	    }
    if (count[0]) {
      plumos_mali_text(renderer, count, count_x, y, entry_scale, r, g, b, 1.0f);
    }
    y += line_height;
    if (y > (float)renderer->height - 34.0f) {
      break;
    }
  }
  if (entry_count == 0) {
    plumos_mali_text(renderer, "NO ENTRIES", 18.0f, y, 2,
                     0.54f, 0.78f, 0.68f, 1.0f);
  }
  if (is_settings_family && (footer1[0] || footer2[0] || wifi_password[0])) {
    plumos_mali_rect(renderer, 0.0f, (float)renderer->height - 74.0f,
                     (float)renderer->width, 74.0f,
                     0.000f, 0.018f, 0.020f, 1.0f);
    plumos_mali_rect(renderer, 0.0f, (float)renderer->height - 76.0f,
                     (float)renderer->width, 2.0f,
                     0.06f, 0.18f, 0.25f, 1.0f);
    if (wifi_password[0]) {
      const char *label = "Password:";
      int label_width = plumos_mali_text_width(label, 2);
      plumos_mali_text(renderer, label, 14.0f, (float)renderer->height - 56.0f,
                       2, 0.64f, 0.82f, 0.92f, 1.0f);
      plumos_mali_text(renderer, wifi_password,
                       14.0f + (float)label_width + 12.0f,
                       (float)renderer->height - 56.0f,
                       2, 1.0f, 0.08f, 0.04f, 1.0f);
    } else if (footer1[0]) {
      plumos_mali_text(renderer, footer1, 14.0f, (float)renderer->height - 56.0f,
                       2, 0.64f, 0.82f, 0.92f, 1.0f);
    }
    if (footer2[0]) {
      plumos_mali_text(renderer, footer2, 14.0f, (float)renderer->height - 34.0f,
                       2, 0.64f, 0.82f, 0.92f, 1.0f);
    }
  }
  (void)status;
  renderer->gl.Finish();
  return renderer->egl.SwapBuffers(renderer->display, renderer->surface) == EGL_TRUE;
}

static int plumos_mali_render_lines(struct plumos_mali_renderer *renderer,
                                    const char lines[][PLUMOS_MALI_RENDER_LINE_MAX],
                                    size_t line_count) {
  if (plumos_mali_has_prefixed_line(lines, line_count, "graphic_mode=")) {
    return plumos_mali_render_lines_graphic(renderer, lines, line_count);
  }

  return plumos_mali_render_lines_tty(renderer, lines, line_count);
}

#endif
