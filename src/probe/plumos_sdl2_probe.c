#include <SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_TIMEOUT_MS 3000

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
  printf("Usage: %s [--timeout-ms MS] [--no-video]\n", argv0);
  printf("Probe SDL2 joystick and GameController visibility for plumOS.\n");
}

int main(int argc, char **argv) {
  int timeout_ms = DEFAULT_TIMEOUT_MS;
  int no_video = 0;
  Uint32 init_flags;
  SDL_version compiled;
  SDL_version linked;
  SDL_Window *window = NULL;
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
  printf("compiled_sdl=%u.%u.%u linked_sdl=%u.%u.%u timeout_ms=%d no_video=%s\n",
         compiled.major, compiled.minor, compiled.patch, linked.major, linked.minor,
         linked.patch, timeout_ms, no_video ? "yes" : "no");
  printf("env SDL_VIDEODRIVER=%s SDL_AUDIODRIVER=%s SDL_JOYSTICK_DEVICE=%s\n",
         getenv("SDL_VIDEODRIVER") ? getenv("SDL_VIDEODRIVER") : "-",
         getenv("SDL_AUDIODRIVER") ? getenv("SDL_AUDIODRIVER") : "-",
         getenv("SDL_JOYSTICK_DEVICE") ? getenv("SDL_JOYSTICK_DEVICE") : "-");

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
    window = SDL_CreateWindow("plumOS SDL2 probe", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, 32, 32, SDL_WINDOW_HIDDEN);
    printf("window create=%s error=\"%s\"\n", window ? "yes" : "no",
           window ? "" : SDL_GetError());
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
  if (window) {
    SDL_DestroyWindow(window);
  }
  SDL_Quit();

  printf("summary joysticks=%d controllers_open=%d joysticks_open=%d controller_events=%d joystick_events=%d\n",
         joystick_count, controller_count, open_joystick_count, controller_events,
         joystick_events);
  return joystick_count > 0 ? 0 : 1;
}
