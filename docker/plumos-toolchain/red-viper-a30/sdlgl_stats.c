#include <stdbool.h>
#include <stdio.h>

#include <GLES2/gl2.h>
#include <SDL2/SDL.h>

void __real_SDL_GL_SwapWindow(SDL_Window *window);

void __wrap_SDL_GL_SwapWindow(SDL_Window *window) {
  static bool printed_info;
  static Uint32 window_start_ms;
  static unsigned frames;
  Uint32 now_ms;

  if (!printed_info) {
    const char *driver = SDL_GetCurrentVideoDriver();
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *version = glGetString(GL_VERSION);
    fprintf(stderr, "red-viper-sdlgl: sdl_video_driver=%s\n",
            driver ? driver : "-");
    fprintf(stderr, "red-viper-sdlgl: gl_renderer=%s\n",
            renderer ? (const char *)renderer : "-");
    fprintf(stderr, "red-viper-sdlgl: gl_version=%s\n",
            version ? (const char *)version : "-");
    printed_info = true;
  }

  __real_SDL_GL_SwapWindow(window);

  now_ms = SDL_GetTicks();
  if (window_start_ms == 0) {
    window_start_ms = now_ms;
  }
  frames++;
  if (now_ms - window_start_ms >= 1000) {
    double elapsed = (double)(now_ms - window_start_ms) / 1000.0;
    fprintf(stderr, "red-viper-sdlgl: present_fps=%.2f frames=%u\n",
            elapsed > 0.0 ? (double)frames / elapsed : 0.0, frames);
    fflush(stderr);
    window_start_ms = now_ms;
    frames = 0;
  }
}
