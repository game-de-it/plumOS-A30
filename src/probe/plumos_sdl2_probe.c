#include <SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_TIMEOUT_MS 3000

typedef unsigned int ProbeGLenum;
typedef unsigned int ProbeGLbitfield;
typedef unsigned char ProbeGLubyte;

#define PROBE_GL_COLOR_BUFFER_BIT 0x00004000
#define PROBE_GL_VENDOR 0x1F00
#define PROBE_GL_RENDERER 0x1F01
#define PROBE_GL_VERSION 0x1F02
#define PROBE_GL_RGBA 0x1908
#define PROBE_GL_UNSIGNED_BYTE 0x1401

typedef const ProbeGLubyte *(*ProbeGLGetStringFn)(ProbeGLenum name);
typedef void (*ProbeGLViewportFn)(int x, int y, int width, int height);
typedef void (*ProbeGLClearColorFn)(float red, float green, float blue, float alpha);
typedef void (*ProbeGLClearFn)(ProbeGLbitfield mask);
typedef void (*ProbeGLReadPixelsFn)(int x, int y, int width, int height, ProbeGLenum format,
                                    ProbeGLenum type, void *pixels);

static void print_video_drivers(void) {
  int count = SDL_GetNumVideoDrivers();

  printf("video_drivers=%d\n", count);
  for (int i = 0; i < count; i++) {
    printf("video_driver index=%d name=\"%s\"\n", i, SDL_GetVideoDriver(i));
  }
}

static void print_render_drivers(void) {
  int count = SDL_GetNumRenderDrivers();

  printf("render_drivers=%d\n", count);
  for (int i = 0; i < count; i++) {
    SDL_RendererInfo info;
    if (SDL_GetRenderDriverInfo(i, &info) == 0) {
      printf("render_driver index=%d name=\"%s\" flags=0x%x max_texture=%dx%d formats=%u",
             i, info.name ? info.name : "-", info.flags, info.max_texture_width,
             info.max_texture_height, info.num_texture_formats);
      for (Uint32 j = 0; j < info.num_texture_formats; j++) {
        printf(" %s", SDL_GetPixelFormatName(info.texture_formats[j]));
      }
      printf("\n");
    } else {
      printf("render_driver index=%d error=\"%s\"\n", i, SDL_GetError());
    }
  }
}

static void print_displays(void) {
  int count = SDL_GetNumVideoDisplays();

  printf("video_displays=%d\n", count);
  for (int i = 0; i < count; i++) {
    SDL_Rect bounds;
    if (SDL_GetDisplayBounds(i, &bounds) == 0) {
      printf("video_display index=%d name=\"%s\" bounds=%d,%d %dx%d\n", i,
             SDL_GetDisplayName(i) ? SDL_GetDisplayName(i) : "-", bounds.x, bounds.y,
             bounds.w, bounds.h);
    } else {
      printf("video_display index=%d error=\"%s\"\n", i, SDL_GetError());
    }
  }
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

static const char *button_name(int button) {
  switch (button) {
  case SDL_CONTROLLER_BUTTON_A:
    return "A";
  case SDL_CONTROLLER_BUTTON_B:
    return "B";
  case SDL_CONTROLLER_BUTTON_X:
    return "X";
  case SDL_CONTROLLER_BUTTON_Y:
    return "Y";
  case SDL_CONTROLLER_BUTTON_BACK:
    return "BACK";
  case SDL_CONTROLLER_BUTTON_GUIDE:
    return "GUIDE";
  case SDL_CONTROLLER_BUTTON_START:
    return "START";
  case SDL_CONTROLLER_BUTTON_LEFTSTICK:
    return "LEFTSTICK";
  case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
    return "RIGHTSTICK";
  case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
    return "LEFTSHOULDER";
  case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
    return "RIGHTSHOULDER";
  case SDL_CONTROLLER_BUTTON_DPAD_UP:
    return "DPAD_UP";
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
    return "DPAD_DOWN";
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    return "DPAD_LEFT";
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
    return "DPAD_RIGHT";
  default:
    return "-";
  }
}

static const char *axis_name(int axis) {
  switch (axis) {
  case SDL_CONTROLLER_AXIS_LEFTX:
    return "LEFTX";
  case SDL_CONTROLLER_AXIS_LEFTY:
    return "LEFTY";
  case SDL_CONTROLLER_AXIS_RIGHTX:
    return "RIGHTX";
  case SDL_CONTROLLER_AXIS_RIGHTY:
    return "RIGHTY";
  case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
    return "TRIGGERLEFT";
  case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
    return "TRIGGERRIGHT";
  default:
    return "-";
  }
}

static void print_guid(SDL_JoystickGUID guid) {
  char text[64];
  SDL_JoystickGetGUIDString(guid, text, sizeof(text));
  printf("%s", text);
}

static void usage(const char *argv0) {
  printf("Usage: %s [--timeout-ms MS] [--no-video] [--graphics-only] [--render-test] [--gl-test] [--window-visible]\n", argv0);
  printf("Probe SDL2 joystick and GameController visibility for plumOS.\n");
}

int main(int argc, char **argv) {
  int timeout_ms = DEFAULT_TIMEOUT_MS;
  int no_video = 0;
  int graphics_only = 0;
  int render_test = 0;
  int gl_test = 0;
  int window_visible = 0;
  int render_ok = 0;
  int gl_ok = 0;
  Uint32 init_flags;
  SDL_version compiled;
  SDL_version linked;
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  SDL_GLContext gl_context = NULL;
  int joystick_count;
  SDL_GameController *controllers[8];
  SDL_Joystick *joysticks[8];
  int controller_count = 0;
  int open_joystick_count = 0;
  int controller_events = 0;
  int joystick_events = 0;
  Uint32 start;

  memset(controllers, 0, sizeof(controllers));
  memset(joysticks, 0, sizeof(joysticks));

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
      if (!parse_int_arg("--timeout-ms", argv[++i], 0, 300000, &timeout_ms)) {
        return 2;
      }
    } else if (strcmp(argv[i], "--no-video") == 0) {
      no_video = 1;
    } else if (strcmp(argv[i], "--graphics-only") == 0) {
      graphics_only = 1;
      timeout_ms = 0;
    } else if (strcmp(argv[i], "--render-test") == 0) {
      render_test = 1;
    } else if (strcmp(argv[i], "--gl-test") == 0) {
      gl_test = 1;
    } else if (strcmp(argv[i], "--window-visible") == 0) {
      window_visible = 1;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  SDL_VERSION(&compiled);
  SDL_GetVersion(&linked);
  printf("plumOS SDL2 probe\n");
  printf("compiled_sdl=%u.%u.%u linked_sdl=%u.%u.%u timeout_ms=%d no_video=%s graphics_only=%s render_test=%s gl_test=%s window_visible=%s\n",
         compiled.major, compiled.minor, compiled.patch, linked.major, linked.minor,
         linked.patch, timeout_ms, no_video ? "yes" : "no", graphics_only ? "yes" : "no",
         render_test ? "yes" : "no", gl_test ? "yes" : "no", window_visible ? "yes" : "no");
  printf("env SDL_VIDEODRIVER=%s SDL_AUDIODRIVER=%s SDL_JOYSTICK_DEVICE=%s\n",
         getenv("SDL_VIDEODRIVER") ? getenv("SDL_VIDEODRIVER") : "-",
         getenv("SDL_AUDIODRIVER") ? getenv("SDL_AUDIODRIVER") : "-",
         getenv("SDL_JOYSTICK_DEVICE") ? getenv("SDL_JOYSTICK_DEVICE") : "-");
  print_video_drivers();

  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

  init_flags = SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;
  if (!no_video) {
    init_flags |= SDL_INIT_VIDEO;
  }
  if (SDL_Init(init_flags) != 0) {
    printf("sdl init=no error=\"%s\"\n", SDL_GetError());
    return 1;
  }
  printf("sdl init=yes current_video_driver=%s current_audio_driver=%s\n",
         SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "-",
         SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "-");
  if (!no_video) {
    print_displays();
    print_render_drivers();
  }

  if (!no_video) {
    Uint32 window_flags = window_visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN;

    if (gl_test) {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
      SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
      window_flags |= SDL_WINDOW_OPENGL;
    }

    window = SDL_CreateWindow("plumOS SDL2 probe", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, 64, 64, window_flags);
    printf("window create=%s visible=%s size=64x64 error=\"%s\"\n", window ? "yes" : "no",
           window_visible ? "yes" : "no", window ? "" : SDL_GetError());
  }

  if (gl_test && window) {
    ProbeGLGetStringFn gl_get_string;
    ProbeGLViewportFn gl_viewport;
    ProbeGLClearColorFn gl_clear_color;
    ProbeGLClearFn gl_clear;
    ProbeGLReadPixelsFn gl_read_pixels;
    unsigned char pixel[4] = { 0, 0, 0, 0 };

    gl_context = SDL_GL_CreateContext(window);
    printf("gl context create=%s error=\"%s\"\n", gl_context ? "yes" : "no",
           gl_context ? "" : SDL_GetError());
    if (gl_context) {
      int make_current = SDL_GL_MakeCurrent(window, gl_context) == 0;
      printf("gl make_current=%s error=\"%s\"\n", make_current ? "yes" : "no",
             make_current ? "" : SDL_GetError());
      gl_get_string = (ProbeGLGetStringFn)SDL_GL_GetProcAddress("glGetString");
      gl_viewport = (ProbeGLViewportFn)SDL_GL_GetProcAddress("glViewport");
      gl_clear_color = (ProbeGLClearColorFn)SDL_GL_GetProcAddress("glClearColor");
      gl_clear = (ProbeGLClearFn)SDL_GL_GetProcAddress("glClear");
      gl_read_pixels = (ProbeGLReadPixelsFn)SDL_GL_GetProcAddress("glReadPixels");
      printf("gl proc glGetString=%s glViewport=%s glClearColor=%s glClear=%s glReadPixels=%s\n",
             gl_get_string ? "yes" : "no", gl_viewport ? "yes" : "no",
             gl_clear_color ? "yes" : "no", gl_clear ? "yes" : "no",
             gl_read_pixels ? "yes" : "no");
      if (make_current && gl_get_string && gl_viewport && gl_clear_color && gl_clear) {
        const ProbeGLubyte *vendor = gl_get_string(PROBE_GL_VENDOR);
        const ProbeGLubyte *renderer_name = gl_get_string(PROBE_GL_RENDERER);
        const ProbeGLubyte *version = gl_get_string(PROBE_GL_VERSION);

        printf("gl vendor=\"%s\" renderer=\"%s\" version=\"%s\"\n",
               vendor ? (const char *)vendor : "-",
               renderer_name ? (const char *)renderer_name : "-",
               version ? (const char *)version : "-");
        gl_viewport(0, 0, 64, 64);
        gl_clear_color(0.20f, 0.55f, 0.85f, 1.0f);
        gl_clear(PROBE_GL_COLOR_BUFFER_BIT);
        if (gl_read_pixels) {
          gl_read_pixels(0, 0, 1, 1, PROBE_GL_RGBA, PROBE_GL_UNSIGNED_BYTE, pixel);
        }
        SDL_ClearError();
        SDL_GL_SwapWindow(window);
        printf("gl swap=yes pixel_rgba=%02x%02x%02x%02x error=\"%s\"\n",
               pixel[0], pixel[1], pixel[2], pixel[3], SDL_GetError());
        gl_ok = 1;
      }
    }
  } else if (gl_test) {
    printf("gl context create=skipped reason=\"no window\"\n");
  }

  if (render_test && window) {
    SDL_RendererInfo info;
    SDL_Rect rect = { 8, 8, 48, 48 };
    SDL_Rect pixel_rect = { 0, 0, 1, 1 };
    Uint32 pixel = 0;
    int read_ok;

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
      printf("render create=default no error=\"%s\"\n", SDL_GetError());
      renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
      printf("render create=software %s error=\"%s\"\n", renderer ? "yes" : "no",
             renderer ? "" : SDL_GetError());
    } else {
      printf("render create=default yes\n");
    }
    if (renderer) {
      memset(&info, 0, sizeof(info));
      if (SDL_GetRendererInfo(renderer, &info) == 0) {
        printf("render info name=\"%s\" flags=0x%x max_texture=%dx%d formats=%u\n",
               info.name ? info.name : "-", info.flags, info.max_texture_width,
               info.max_texture_height, info.num_texture_formats);
      } else {
        printf("render info error=\"%s\"\n", SDL_GetError());
      }
      SDL_SetRenderDrawColor(renderer, 0x10, 0x20, 0x40, 0xff);
      SDL_RenderClear(renderer);
      SDL_SetRenderDrawColor(renderer, 0x40, 0xc0, 0x80, 0xff);
      SDL_RenderFillRect(renderer, &rect);
      SDL_RenderPresent(renderer);
      read_ok = SDL_RenderReadPixels(renderer, &pixel_rect, SDL_PIXELFORMAT_ARGB8888,
                                     &pixel, (int)sizeof(pixel)) == 0;
      printf("render present=yes readpixels=%s pixel_argb8888=0x%08x error=\"%s\"\n",
             read_ok ? "yes" : "no", pixel, read_ok ? "" : SDL_GetError());
      render_ok = 1;
    }
  } else if (render_test) {
    printf("render create=skipped reason=\"no window\"\n");
  }

  joystick_count = SDL_NumJoysticks();
  printf("joysticks=%d\n", joystick_count);
  for (int i = 0; i < joystick_count; i++) {
    const char *name = SDL_JoystickNameForIndex(i);
    SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
    int is_controller = SDL_IsGameController(i);

    printf("device index=%d name=\"%s\" guid=", i, name ? name : "-");
    print_guid(guid);
    printf(" is_controller=%s\n", is_controller ? "yes" : "no");

    if (is_controller && controller_count < (int)(sizeof(controllers) / sizeof(controllers[0]))) {
      SDL_GameController *controller = SDL_GameControllerOpen(i);
      printf("controller open index=%d %s\n", i, controller ? "yes" : SDL_GetError());
      if (controller) {
        SDL_Joystick *joy = SDL_GameControllerGetJoystick(controller);
        char *mapping = SDL_GameControllerMapping(controller);
        controllers[controller_count++] = controller;
        printf("controller info index=%d name=\"%s\" attached=%s axes=%d buttons=%d hats=%d mapping=\"%s\"\n",
               i, SDL_GameControllerName(controller) ? SDL_GameControllerName(controller) : "-",
               SDL_GameControllerGetAttached(controller) ? "yes" : "no",
               joy ? SDL_JoystickNumAxes(joy) : -1, joy ? SDL_JoystickNumButtons(joy) : -1,
               joy ? SDL_JoystickNumHats(joy) : -1, mapping ? mapping : "-");
        SDL_free(mapping);
      }
    } else if (open_joystick_count < (int)(sizeof(joysticks) / sizeof(joysticks[0]))) {
      SDL_Joystick *joy = SDL_JoystickOpen(i);
      printf("joystick open index=%d %s\n", i, joy ? "yes" : SDL_GetError());
      if (joy) {
        joysticks[open_joystick_count++] = joy;
        printf("joystick info index=%d name=\"%s\" axes=%d buttons=%d hats=%d balls=%d attached=%s\n",
               i, SDL_JoystickName(joy) ? SDL_JoystickName(joy) : "-",
               SDL_JoystickNumAxes(joy), SDL_JoystickNumButtons(joy),
               SDL_JoystickNumHats(joy), SDL_JoystickNumBalls(joy),
               SDL_JoystickGetAttached(joy) ? "yes" : "no");
      }
    }
  }

  start = SDL_GetTicks();
  while ((int)(SDL_GetTicks() - start) < timeout_ms) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_CONTROLLERDEVICEADDED:
        printf("event controller_added which=%d\n", event.cdevice.which);
        controller_events++;
        break;
      case SDL_CONTROLLERDEVICEREMOVED:
        printf("event controller_removed which=%d\n", event.cdevice.which);
        controller_events++;
        break;
      case SDL_CONTROLLERAXISMOTION:
        printf("event controller_axis which=%d axis=%d axis_name=%s value=%d\n",
               event.caxis.which, event.caxis.axis, axis_name(event.caxis.axis),
               event.caxis.value);
        controller_events++;
        break;
      case SDL_CONTROLLERBUTTONDOWN:
      case SDL_CONTROLLERBUTTONUP:
        printf("event controller_button which=%d button=%d button_name=%s value=%d\n",
               event.cbutton.which, event.cbutton.button, button_name(event.cbutton.button),
               event.cbutton.state == SDL_PRESSED ? 1 : 0);
        controller_events++;
        break;
      case SDL_JOYDEVICEADDED:
        printf("event joy_added which=%d\n", event.jdevice.which);
        joystick_events++;
        break;
      case SDL_JOYDEVICEREMOVED:
        printf("event joy_removed which=%d\n", event.jdevice.which);
        joystick_events++;
        break;
      case SDL_JOYAXISMOTION:
        printf("event joy_axis which=%d axis=%d value=%d\n", event.jaxis.which,
               event.jaxis.axis, event.jaxis.value);
        joystick_events++;
        break;
      case SDL_JOYBUTTONDOWN:
      case SDL_JOYBUTTONUP:
        printf("event joy_button which=%d button=%d value=%d\n", event.jbutton.which,
               event.jbutton.button, event.jbutton.state == SDL_PRESSED ? 1 : 0);
        joystick_events++;
        break;
      case SDL_JOYHATMOTION:
        printf("event joy_hat which=%d hat=%d value=%u\n", event.jhat.which,
               event.jhat.hat, (unsigned int)event.jhat.value);
        joystick_events++;
        break;
      default:
        break;
      }
    }
    SDL_Delay(10);
  }

  for (int i = 0; i < controller_count; i++) {
    SDL_GameControllerClose(controllers[i]);
  }
  for (int i = 0; i < open_joystick_count; i++) {
    SDL_JoystickClose(joysticks[i]);
  }
  if (renderer) {
    SDL_DestroyRenderer(renderer);
  }
  if (gl_context) {
    SDL_GL_DeleteContext(gl_context);
  }
  if (window) {
    SDL_DestroyWindow(window);
  }
  SDL_Quit();

  printf("summary joysticks=%d controllers_open=%d joysticks_open=%d controller_events=%d joystick_events=%d\n",
         joystick_count, controller_count, open_joystick_count, controller_events,
         joystick_events);
  if (graphics_only) {
    if (gl_test) {
      return gl_ok ? 0 : 1;
    }
    return render_test ? (render_ok ? 0 : 1) : 0;
  }
  return joystick_count > 0 ? 0 : 1;
}
