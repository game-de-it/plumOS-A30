#ifndef PLUMOS_MALI_RENDERER_H
#define PLUMOS_MALI_RENDERER_H

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
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
#define GL_FLOAT 0x1406
#define GL_TRIANGLE_STRIP 0x0005
#define GL_VERTEX_SHADER 0x8b31
#define GL_FRAGMENT_SHADER 0x8b30
#define GL_COMPILE_STATUS 0x8b81
#define GL_LINK_STATUS 0x8b82

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
  void (*EnableVertexAttribArray)(GLuint index);
  void (*VertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized,
                              GLsizei stride, const void *pointer);
  void (*DrawArrays)(GLenum mode, GLint first, GLsizei count);
  void (*Finish)(void);
  void (*DeleteProgram)(GLuint program);
};

struct plumos_mali_renderer {
  struct plumos_mali_egl_api egl;
  struct plumos_mali_gl_api gl;
  int fb_fd;
  int width;
  int height;
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
  GLuint program;
  GLint color_uniform;
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
  PLUMOS_LOAD_GL(renderer, EnableVertexAttribArray, "glEnableVertexAttribArray", error,
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

static int plumos_mali_renderer_init(struct plumos_mali_renderer *renderer, const char *fb_path,
                                     const char *egl_path, const char *gles_path,
                                     char *error, size_t error_size) {
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
    renderer->width = width;
    renderer->height = height;
  }
  renderer->gl.Viewport(0, 0, renderer->width, renderer->height);
  if (!plumos_mali_setup_program(renderer, error, error_size)) {
    return 0;
  }
  return 1;
}

static void plumos_mali_renderer_shutdown(struct plumos_mali_renderer *renderer) {
  if (!renderer) {
    return;
  }
  if (renderer->display != EGL_NO_DISPLAY) {
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

  if (ch >= 'a' && ch <= 'z') {
    ch = (char)(ch - 'a' + 'A');
  }
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
  default:
    return unknown;
  }
}

static void plumos_mali_rect(struct plumos_mali_renderer *renderer, float x, float y,
                             float w, float h, float red, float green, float blue,
                             float alpha) {
  GLfloat x0 = (x / (float)renderer->width) * 2.0f - 1.0f;
  GLfloat x1 = ((x + w) / (float)renderer->width) * 2.0f - 1.0f;
  GLfloat y0 = 1.0f - (y / (float)renderer->height) * 2.0f;
  GLfloat y1 = 1.0f - ((y + h) / (float)renderer->height) * 2.0f;
  GLfloat verts[8] = {x0, y0, x1, y0, x0, y1, x1, y1};

  renderer->gl.Uniform4f(renderer->color_uniform, red, green, blue, alpha);
  renderer->gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
  renderer->gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void plumos_mali_text(struct plumos_mali_renderer *renderer, const char *text,
                             float x, float y, int scale, float red, float green,
                             float blue, float alpha) {
  const unsigned char *rows;
  int glyph_w = 5 * scale;
  int step = 6 * scale;
  int max_chars;
  int i;

  if (!text) {
    return;
  }
  max_chars = (renderer->width - (int)x - 8) / step;
  if (max_chars < 0) {
    return;
  }
  for (i = 0; text[i] && i < max_chars; i++) {
    int row;
    unsigned char ch = (unsigned char)text[i];
    if (ch < 32 || ch > 126) {
      ch = '?';
    }
    rows = plumos_mali_glyph((char)ch);
    for (row = 0; row < 7; row++) {
      int col;
      for (col = 0; col < 5; col++) {
        if (rows[row] & (1u << (4 - col))) {
          plumos_mali_rect(renderer, x + (float)(i * step + col * scale),
                           y + (float)(row * scale), (float)scale, (float)scale,
                           red, green, blue, alpha);
        }
      }
    }
    (void)glyph_w;
  }
}

static int plumos_mali_render_lines(struct plumos_mali_renderer *renderer,
                                    const char lines[][160], size_t line_count) {
  size_t i;
  float margin = 12.0f;
  float y = 14.0f;
  int scale = 2;
  float line_height = 16.0f;
  size_t max_lines;

  renderer->gl.Viewport(0, 0, renderer->width, renderer->height);
  renderer->gl.UseProgram(renderer->program);
  renderer->gl.ClearColor(0.015f, 0.018f, 0.020f, 1.0f);
  renderer->gl.Clear(GL_COLOR_BUFFER_BIT);

  plumos_mali_rect(renderer, 0.0f, 0.0f, (float)renderer->width, 34.0f,
                   0.035f, 0.070f, 0.080f, 1.0f);
  plumos_mali_rect(renderer, 0.0f, 0.0f, 5.0f, (float)renderer->height,
                   0.95f, 0.55f, 0.12f, 1.0f);

  if (line_count > 0) {
    plumos_mali_text(renderer, lines[0], margin, y, scale, 0.70f, 0.95f, 0.92f, 1.0f);
  }

  y = 44.0f;
  max_lines = (size_t)((renderer->height - y - 10.0f) / line_height);
  for (i = 1; i < line_count && i <= max_lines; i++) {
    const char *line = lines[i];
    float r = 0.82f;
    float g = 0.86f;
    float b = 0.86f;
    if (line[0] == '>') {
      plumos_mali_rect(renderer, 8.0f, y - 4.0f, (float)renderer->width - 16.0f,
                       line_height, 0.13f, 0.18f, 0.16f, 1.0f);
      r = 1.0f;
      g = 0.92f;
      b = 0.58f;
    } else if (strstr(line, "status:") == line || strstr(line, "source:") == line) {
      r = 0.56f;
      g = 0.78f;
      b = 0.93f;
    }
    plumos_mali_text(renderer, line, margin, y, scale, r, g, b, 1.0f);
    y += line_height;
  }
  renderer->gl.Finish();
  return renderer->egl.SwapBuffers(renderer->display, renderer->surface) == EGL_TRUE;
}

#endif
