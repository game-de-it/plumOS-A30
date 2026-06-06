#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<linux/input.h>)
#include <linux/input.h>
#define PLUMOS_HAS_LINUX_INPUT 1
#endif
#endif

#ifndef PLUMOS_HAS_LINUX_INPUT
#define EV_KEY 0x01
struct input_event {
  struct timeval time;
  unsigned short type;
  unsigned short code;
  int value;
};
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef KEY_ESC
#define KEY_ESC 1
#endif
#ifndef KEY_6
#define KEY_6 7
#endif
#ifndef KEY_8
#define KEY_8 9
#endif
#ifndef KEY_9
#define KEY_9 10
#endif
#ifndef KEY_BACKSPACE
#define KEY_BACKSPACE 14
#endif
#ifndef KEY_TAB
#define KEY_TAB 15
#endif
#ifndef KEY_Q
#define KEY_Q 16
#endif
#ifndef KEY_E
#define KEY_E 18
#endif
#ifndef KEY_T
#define KEY_T 20
#endif
#ifndef KEY_R
#define KEY_R 19
#endif
#ifndef KEY_LEFTCTRL
#define KEY_LEFTCTRL 29
#endif
#ifndef KEY_LEFTSHIFT
#define KEY_LEFTSHIFT 42
#endif
#ifndef KEY_LEFTALT
#define KEY_LEFTALT 56
#endif
#ifndef KEY_Z
#define KEY_Z 44
#endif
#ifndef KEY_X
#define KEY_X 45
#endif
#ifndef KEY_SPACE
#define KEY_SPACE 57
#endif
#ifndef KEY_HOME
#define KEY_HOME 102
#endif
#ifndef KEY_VOLUMEDOWN
#define KEY_VOLUMEDOWN 114
#endif
#ifndef KEY_VOLUMEUP
#define KEY_VOLUMEUP 115
#endif
#ifndef KEY_POWER
#define KEY_POWER 116
#endif
#ifndef KEY_UP
#define KEY_UP 103
#endif
#ifndef KEY_LEFT
#define KEY_LEFT 105
#endif
#ifndef KEY_RIGHT
#define KEY_RIGHT 106
#endif
#ifndef KEY_DOWN
#define KEY_DOWN 108
#endif
#ifndef KEY_RIGHTCTRL
#define KEY_RIGHTCTRL 97
#endif
#ifndef KEY_ENTER
#define KEY_ENTER 28
#endif

#ifndef BTN_SOUTH
#define BTN_SOUTH 304
#endif
#ifndef BTN_EAST
#define BTN_EAST 305
#endif
#ifndef BTN_NORTH
#define BTN_NORTH 307
#endif
#ifndef BTN_WEST
#define BTN_WEST 308
#endif
#ifndef BTN_TL
#define BTN_TL 310
#endif
#ifndef BTN_TR
#define BTN_TR 311
#endif
#ifndef BTN_TL2
#define BTN_TL2 312
#endif
#ifndef BTN_TR2
#define BTN_TR2 313
#endif
#ifndef BTN_SELECT
#define BTN_SELECT 314
#endif
#ifndef BTN_START
#define BTN_START 315
#endif
#ifndef BTN_MODE
#define BTN_MODE 316
#endif
#ifndef KEY_SELECT
#define KEY_SELECT 0x161
#endif
#ifndef KEY_MENU
#define KEY_MENU 139
#endif

#define UI_MAX_TOP 128
#define UI_MAX_ROMS 256
#define UI_MAX_MENU 64
#define UI_MAX_SETTINGS 64
#define UI_COMMAND_MAX 8192
#define UI_PATH_MAX 1024

struct top_entry {
  char id[64];
  char display_name[128];
  char default_launch_profile[128];
  long rom_count;
  int pinned;
  int virtual_entry;
};

struct rom_entry {
  char system_id[64];
  char title[256];
  char relative_path[UI_PATH_MAX];
  char path[UI_PATH_MAX];
  char launch_profile[128];
  char detail[256];
  int resume_available;
};

struct menu_entry {
  char id[64];
  char display_name[128];
  char kind[64];
  char action[128];
  int confirm;
};

struct setting_entry {
  char id[64];
  char display_name[128];
  char value[256];
};

struct theme_state {
  int loaded;
  int fallback;
  int force_no_icons;
  long line_height;
  char id[64];
  char display_name[128];
  char path[PATH_MAX];
  char layout_preset[64];
  char font_ui[UI_PATH_MAX];
  char font_fallback[64];
  char placeholder_thumbnail[UI_PATH_MAX];
  char status[256];
};

struct device_settings {
  int loaded;
  int wpa_loaded;
  long volume;
  long mute;
  long bgm_volume;
  long brightness;
  long lumination;
  long contrast;
  long hue;
  long saturation;
  long wifi_enabled;
  long cpu_freq_mode;
  char keymap[128];
  char language[64];
  char stock_theme[UI_PATH_MAX];
  char brightness_backend[128];
  char volume_backend[128];
  char wifi_state[64];
  char wifi_ip[64];
  char wifi_rssi[64];
  char wifi_linkspeed[64];
  char wifi_frequency[64];
  char status[256];
};

struct frontend_settings {
  int show_empty_systems;
  int show_favorites_on_top;
  char boot_resume_mode[32];
  char ui_mode[32];
  char top_mode[32];
  char rom_mode[32];
  char theme_id[64];
  char sort_systems[64];
  char sort_roms[64];
  char rom_scan_policy[64];
};

enum ui_screen {
  SCREEN_TOP = 0,
  SCREEN_ROMS = 1,
  SCREEN_START_MENU = 2,
  SCREEN_FAVORITES = 3,
  SCREEN_RECENT = 4,
  SCREEN_SETTINGS = 5
};

enum ui_action {
  ACTION_NONE = 0,
  ACTION_UP,
  ACTION_DOWN,
  ACTION_LEFT,
  ACTION_RIGHT,
  ACTION_A,
  ACTION_B,
  ACTION_START,
  ACTION_SELECT,
  ACTION_FUNCTION,
  ACTION_QUIT
};

struct ui_state {
  const char *sdcard_root;
  char plumos_root[PATH_MAX];
  char top_cache_path[PATH_MAX];
  char settings_path[PATH_MAX];
  char system_config_path[PATH_MAX];
  char wpa_status_path[PATH_MAX];
  char menus_path[PATH_MAX];
  char favorites_path[PATH_MAX];
  char recent_path[PATH_MAX];
  int show_all;
  int refresh;
  int no_clear;
  int once;
  int timeout_sec;
  enum ui_screen screen;
  enum ui_screen back_screen;
  size_t top_cursor;
  size_t rom_cursor;
  size_t menu_cursor;
  size_t settings_cursor;
  struct top_entry top_entries[UI_MAX_TOP];
  size_t top_count;
  struct rom_entry rom_entries[UI_MAX_ROMS];
  size_t rom_count;
  struct menu_entry menu_entries[UI_MAX_MENU];
  size_t menu_count;
  struct setting_entry setting_entries[UI_MAX_SETTINGS];
  size_t setting_count;
  struct theme_state theme;
  struct device_settings device;
  char input_event_path[PATH_MAX];
  char current_system_id[64];
  char current_system_name[128];
  char status[256];
};

static int copy_string(char *out, size_t out_size, const char *in) {
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

static int append_string(char *out, size_t out_size, size_t *pos, const char *in) {
  size_t len;
  if (!out || !pos || !in) {
    return 0;
  }
  len = strlen(in);
  if (*pos + len + 1 > out_size) {
    return 0;
  }
  memcpy(out + *pos, in, len);
  *pos += len;
  out[*pos] = '\0';
  return 1;
}

static int append_shell_quoted(char *out, size_t out_size, size_t *pos, const char *in) {
  const char *p;
  if (!append_string(out, out_size, pos, "'")) {
    return 0;
  }
  for (p = in; p && *p; p++) {
    char c[2];
    if (*p == '\'') {
      if (!append_string(out, out_size, pos, "'\\''")) {
        return 0;
      }
    } else {
      c[0] = *p;
      c[1] = '\0';
      if (!append_string(out, out_size, pos, c)) {
        return 0;
      }
    }
  }
  return append_string(out, out_size, pos, "'");
}

static int join_path(char *out, size_t out_size, const char *a, const char *b) {
  size_t len_a;
  size_t len_b;
  size_t pos = 0;

  if (!out || out_size == 0 || !a || !b) {
    return 0;
  }
  out[0] = '\0';
  if (b[0] == '/') {
    return copy_string(out, out_size, b);
  }

  len_a = strlen(a);
  len_b = strlen(b);
  if (len_a + (len_a && a[len_a - 1] != '/' ? 1 : 0) + len_b + 1 > out_size) {
    return 0;
  }
  if (!append_string(out, out_size, &pos, a)) {
    return 0;
  }
  if (pos > 0 && out[pos - 1] != '/') {
    if (!append_string(out, out_size, &pos, "/")) {
      return 0;
    }
  }
  return append_string(out, out_size, &pos, b);
}

static int file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int dir_has_entries(const char *path) {
  DIR *dir;
  struct dirent *entry;

  dir = opendir(path);
  if (!dir) {
    return 0;
  }
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      closedir(dir);
      return 1;
    }
  }
  closedir(dir);
  return 0;
}

static int valid_system_id(const char *s) {
  if (!s || !s[0]) {
    return 0;
  }
  while (*s) {
    unsigned char c = (unsigned char)*s++;
    if (!(isalnum(c) || c == '_' || c == '-')) {
      return 0;
    }
  }
  return 1;
}

static char *read_file(const char *path, size_t *size_out) {
  FILE *f;
  char *buf;
  long size;

  f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);

  buf = (char *)calloc((size_t)size + 1, 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
    free(buf);
    fclose(f);
    return NULL;
  }
  fclose(f);
  if (size_out) {
    *size_out = (size_t)size;
  }
  return buf;
}

static int read_key_value_file(const char *path, const char *key, char *out, size_t out_size) {
  char *text;
  char *p;
  size_t key_len;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!path || !key || !key[0]) {
    return 0;
  }
  text = read_file(path, NULL);
  if (!text) {
    return 0;
  }
  key_len = strlen(key);
  p = text;
  while (p && *p) {
    char *line = p;
    char *next = strchr(p, '\n');
    if (next) {
      *next = '\0';
      p = next + 1;
    } else {
      p += strlen(p);
    }
    if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
      int ok = copy_string(out, out_size, line + key_len + 1);
      free(text);
      return ok;
    }
  }
  free(text);
  return 0;
}

static const char *skip_ws_range(const char *p, const char *end) {
  while (p < end && isspace((unsigned char)*p)) {
    p++;
  }
  return p;
}

static int json_decode_string(const char **cursor, const char *end, char *out, size_t out_size) {
  const char *p = *cursor;
  size_t n = 0;

  if (p >= end || *p != '"' || out_size == 0) {
    return 0;
  }
  p++;
  while (p < end && *p != '"') {
    char c = *p++;
    if (c == '\\' && p < end) {
      c = *p++;
      switch (c) {
      case '"':
      case '\\':
      case '/':
        break;
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      default:
        break;
      }
    }
    if (n + 1 < out_size) {
      out[n++] = c;
    }
  }
  if (p >= end || *p != '"') {
    out[0] = '\0';
    return 0;
  }
  out[n] = '\0';
  *cursor = p + 1;
  return 1;
}

static int json_match_container(const char *start, const char *end, char open_ch,
                                char close_ch, const char **body_start,
                                const char **body_end, const char **after_out) {
  const char *p = start;
  int depth = 0;
  int in_string = 0;
  int escape = 0;

  if (p >= end || *p != open_ch) {
    return 0;
  }
  while (p < end) {
    if (escape) {
      escape = 0;
      p++;
      continue;
    }
    if (in_string && *p == '\\') {
      escape = 1;
      p++;
      continue;
    }
    if (*p == '"') {
      in_string = !in_string;
      p++;
      continue;
    }
    if (!in_string && *p == open_ch) {
      if (depth == 0 && body_start) {
        *body_start = p + 1;
      }
      depth++;
    } else if (!in_string && *p == close_ch) {
      depth--;
      if (depth == 0) {
        if (body_end) {
          *body_end = p;
        }
        if (after_out) {
          *after_out = p + 1;
        }
        return 1;
      }
    }
    p++;
  }
  return 0;
}

static int json_find_key_value(const char *json, const char *end, const char *key,
                               const char **value_out) {
  char pattern[128];
  const char *p = json;

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  while (p < end && (p = strstr(p, pattern)) != NULL && p < end) {
    const char *q = p + strlen(pattern);
    q = skip_ws_range(q, end);
    if (q < end && *q == ':') {
      q++;
      *value_out = skip_ws_range(q, end);
      return 1;
    }
    p++;
  }
  return 0;
}

static int json_find_array(const char *json, const char *end, const char *key,
                           const char **body_start, const char **body_end) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return 0;
  }
  return json_match_container(value, end, '[', ']', body_start, body_end, NULL);
}

static int json_find_object(const char *json, const char *end, const char *key,
                            const char **body_start, const char **body_end) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return 0;
  }
  return json_match_container(value, end, '{', '}', body_start, body_end, NULL);
}

static int json_next_object(const char **cursor, const char *end,
                            const char **object_start, const char **object_end) {
  const char *p = *cursor;
  const char *body_start;
  const char *body_end;
  const char *after;

  while (p < end && *p != '{') {
    p++;
  }
  if (p >= end) {
    return 0;
  }
  if (!json_match_container(p, end, '{', '}', &body_start, &body_end, &after)) {
    return 0;
  }
  (void)body_start;
  *object_start = p;
  *object_end = after;
  *cursor = after;
  return 1;
}

static int json_get_string(const char *json, const char *end, const char *key,
                           char *out, size_t out_size) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    if (out_size > 0) {
      out[0] = '\0';
    }
    return 0;
  }
  return json_decode_string(&value, end, out, out_size);
}

static long json_get_long(const char *json, const char *end, const char *key, long default_value) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return default_value;
  }
  return strtol(value, NULL, 10);
}

static int json_get_bool(const char *json, const char *end, const char *key, int default_value) {
  const char *value;
  if (!json_find_key_value(json, end, key, &value)) {
    return default_value;
  }
  if ((size_t)(end - value) >= 4 && strncmp(value, "true", 4) == 0) {
    return 1;
  }
  if ((size_t)(end - value) >= 5 && strncmp(value, "false", 5) == 0) {
    return 0;
  }
  return default_value;
}

static int count_json_array_objects(const char *path, const char *array_key) {
  char *json;
  size_t json_size;
  const char *start;
  const char *end;
  const char *cursor;
  int count = 0;

  if (!file_exists(path)) {
    return 0;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, array_key, &start, &end)) {
    free(json);
    return 0;
  }
  cursor = start;
  while (count < 9999) {
    const char *obj_start;
    const char *obj_end;
    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }
    (void)obj_start;
    (void)obj_end;
    count++;
  }
  free(json);
  return count;
}

static int run_scanner(const char *plumos_root, const char *sdcard_root, const char *system_id) {
  char scanner[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!join_path(scanner, sizeof(scanner), plumos_root, "bin/plumos-library-scan")) {
    return 0;
  }
  if (!file_exists(scanner)) {
    return 0;
  }
  if (system_id && !valid_system_id(system_id)) {
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, scanner)) {
    return 0;
  }
  if (system_id) {
    if (!append_string(cmd, sizeof(cmd), &pos, " --on-enter ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, system_id)) {
      return 0;
    }
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " >/dev/null")) {
    return 0;
  }

  rc = system(cmd);
  return rc == 0;
}

static int load_settings(const char *path, struct frontend_settings *settings) {
  char *json;
  size_t json_size;

  memset(settings, 0, sizeof(*settings));
  copy_string(settings->boot_resume_mode, sizeof(settings->boot_resume_mode), "off");
  if (!file_exists(path)) {
    return 1;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  settings->show_empty_systems = json_get_bool(json, json + json_size, "show_empty_systems", 0);
  settings->show_favorites_on_top =
      json_get_bool(json, json + json_size, "show_favorites_on_top", 0);
  json_get_string(json, json + json_size, "boot_resume_mode", settings->boot_resume_mode,
                  sizeof(settings->boot_resume_mode));
  json_get_string(json, json + json_size, "ui_mode", settings->ui_mode,
                  sizeof(settings->ui_mode));
  json_get_string(json, json + json_size, "top_mode", settings->top_mode,
                  sizeof(settings->top_mode));
  json_get_string(json, json + json_size, "rom_mode", settings->rom_mode,
                  sizeof(settings->rom_mode));
  json_get_string(json, json + json_size, "theme_id", settings->theme_id,
                  sizeof(settings->theme_id));
  json_get_string(json, json + json_size, "sort_systems", settings->sort_systems,
                  sizeof(settings->sort_systems));
  json_get_string(json, json + json_size, "sort_roms", settings->sort_roms,
                  sizeof(settings->sort_roms));
  json_get_string(json, json + json_size, "rom_scan_policy", settings->rom_scan_policy,
                  sizeof(settings->rom_scan_policy));
  free(json);
  return 1;
}

static void init_device_settings(struct device_settings *device) {
  memset(device, 0, sizeof(*device));
  device->volume = -1;
  device->mute = -1;
  device->bgm_volume = -1;
  device->brightness = -1;
  device->lumination = -1;
  device->contrast = -1;
  device->hue = -1;
  device->saturation = -1;
  device->wifi_enabled = -1;
  device->cpu_freq_mode = -1;
  copy_string(device->brightness_backend, sizeof(device->brightness_backend), "unresolved");
  copy_string(device->volume_backend, sizeof(device->volume_backend), "unresolved");
  copy_string(device->status, sizeof(device->status), "read-only inventory");
}

static void load_device_runtime_status(struct ui_state *ui) {
  struct device_settings *device = &ui->device;

  if (read_key_value_file(ui->wpa_status_path, "wpa_state", device->wifi_state,
                          sizeof(device->wifi_state))) {
    device->wpa_loaded = 1;
  }
  if (read_key_value_file(ui->wpa_status_path, "ip_address", device->wifi_ip,
                          sizeof(device->wifi_ip))) {
    device->wpa_loaded = 1;
  }
  read_key_value_file(ui->wpa_status_path, "RSSI", device->wifi_rssi,
                      sizeof(device->wifi_rssi));
  read_key_value_file(ui->wpa_status_path, "LINKSPEED", device->wifi_linkspeed,
                      sizeof(device->wifi_linkspeed));
  read_key_value_file(ui->wpa_status_path, "FREQUENCY", device->wifi_frequency,
                      sizeof(device->wifi_frequency));
}

static int load_device_settings(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *json_end;

  init_device_settings(&ui->device);

  if (file_exists("/usr/bin/amixer") || file_exists("/bin/amixer")) {
    copy_string(ui->device.volume_backend, sizeof(ui->device.volume_backend),
                "system.json + amixer candidate");
  } else {
    copy_string(ui->device.volume_backend, sizeof(ui->device.volume_backend),
                "system.json only");
  }

  if (dir_has_entries("/sys/class/backlight")) {
    copy_string(ui->device.brightness_backend, sizeof(ui->device.brightness_backend),
                "sysfs backlight candidate");
  } else if (dir_has_entries("/sys/class/lcd")) {
    copy_string(ui->device.brightness_backend, sizeof(ui->device.brightness_backend),
                "system.json + lcd sysfs candidate");
  } else {
    copy_string(ui->device.brightness_backend, sizeof(ui->device.brightness_backend),
                "system.json only");
  }

  load_device_runtime_status(ui);

  json = read_file(ui->system_config_path, &json_size);
  if (!json) {
    copy_string(ui->device.status, sizeof(ui->device.status), "system config missing");
    return 1;
  }

  json_end = json + json_size;
  ui->device.loaded = 1;
  ui->device.volume = json_get_long(json, json_end, "vol", -1);
  ui->device.mute = json_get_long(json, json_end, "mute", -1);
  ui->device.bgm_volume = json_get_long(json, json_end, "bgmvol", -1);
  ui->device.brightness = json_get_long(json, json_end, "brightness", -1);
  ui->device.lumination = json_get_long(json, json_end, "lumination", -1);
  ui->device.contrast = json_get_long(json, json_end, "contrast", -1);
  ui->device.hue = json_get_long(json, json_end, "hue", -1);
  ui->device.saturation = json_get_long(json, json_end, "saturation", -1);
  ui->device.wifi_enabled = json_get_long(json, json_end, "wifi", -1);
  ui->device.cpu_freq_mode = json_get_long(json, json_end, "cpufreq", -1);
  json_get_string(json, json_end, "keymap", ui->device.keymap,
                  sizeof(ui->device.keymap));
  json_get_string(json, json_end, "language", ui->device.language,
                  sizeof(ui->device.language));
  json_get_string(json, json_end, "theme", ui->device.stock_theme,
                  sizeof(ui->device.stock_theme));
  free(json);
  return 1;
}

static int build_theme_path(char *out, size_t out_size, const char *plumos_root,
                            const char *theme_id) {
  char dir[PATH_MAX];
  char theme_dir[PATH_MAX];

  if (!valid_system_id(theme_id)) {
    return 0;
  }
  if (!join_path(dir, sizeof(dir), plumos_root, "themes")) {
    return 0;
  }
  if (!join_path(theme_dir, sizeof(theme_dir), dir, theme_id)) {
    return 0;
  }
  return join_path(out, out_size, theme_dir, "theme.json");
}

static void init_theme_state(struct theme_state *theme, const char *theme_id,
                             const char *theme_path) {
  memset(theme, 0, sizeof(*theme));
  theme->fallback = 1;
  theme->line_height = 14;
  theme->force_no_icons = 1;
  copy_string(theme->id, sizeof(theme->id), theme_id && theme_id[0] ? theme_id : "default");
  copy_string(theme->display_name, sizeof(theme->display_name), "Built-in Text");
  copy_string(theme->layout_preset, sizeof(theme->layout_preset), "compact_text");
  copy_string(theme->font_fallback, sizeof(theme->font_fallback), "builtin");
  copy_string(theme->status, sizeof(theme->status), "builtin text fallback");
  if (theme_path) {
    copy_string(theme->path, sizeof(theme->path), theme_path);
  }
}

static int theme_behavior_is_blocked(const char *json, const char *end) {
  const char *policy_start;
  const char *policy_end;

  if (!json_find_object(json, end, "behavior_policy", &policy_start, &policy_end)) {
    return 0;
  }
  if (json_get_bool(policy_start, policy_end, "theme_may_change_input", 0) ||
      json_get_bool(policy_start, policy_end, "theme_may_change_menu_actions", 0) ||
      json_get_bool(policy_start, policy_end, "theme_may_change_launch_profiles", 0) ||
      json_get_bool(policy_start, policy_end, "theme_may_change_rom_scan", 0) ||
      json_get_bool(policy_start, policy_end, "theme_may_change_resume", 0)) {
    return 1;
  }
  return 0;
}

static int load_theme_state(struct ui_state *ui, const char *theme_id) {
  char theme_path[PATH_MAX];
  char *json;
  size_t json_size;
  const char *assets_start;
  const char *assets_end;
  const char *text_start;
  const char *text_end;
  const char *json_end;

  if (!theme_id || !theme_id[0]) {
    theme_id = "default";
  }
  if (!build_theme_path(theme_path, sizeof(theme_path), ui->plumos_root, theme_id)) {
    init_theme_state(&ui->theme, theme_id, "");
    copy_string(ui->theme.status, sizeof(ui->theme.status), "invalid theme id; builtin fallback");
    return 1;
  }

  init_theme_state(&ui->theme, theme_id, theme_path);
  json = read_file(theme_path, &json_size);
  if (!json) {
    copy_string(ui->theme.status, sizeof(ui->theme.status), "theme missing; builtin fallback");
    return 1;
  }
  json_end = json + json_size;
  ui->theme.loaded = 1;
  ui->theme.fallback = 0;

  json_get_string(json, json_end, "id", ui->theme.id, sizeof(ui->theme.id));
  json_get_string(json, json_end, "display_name", ui->theme.display_name,
                  sizeof(ui->theme.display_name));
  json_get_string(json, json_end, "layout_preset", ui->theme.layout_preset,
                  sizeof(ui->theme.layout_preset));
  if (!ui->theme.display_name[0]) {
    copy_string(ui->theme.display_name, sizeof(ui->theme.display_name), ui->theme.id);
  }
  if (!ui->theme.layout_preset[0]) {
    copy_string(ui->theme.layout_preset, sizeof(ui->theme.layout_preset), "compact_text");
  }

  if (json_find_object(json, json_end, "assets", &assets_start, &assets_end)) {
    json_get_string(assets_start, assets_end, "font_ui", ui->theme.font_ui,
                    sizeof(ui->theme.font_ui));
    json_get_string(assets_start, assets_end, "font_fallback", ui->theme.font_fallback,
                    sizeof(ui->theme.font_fallback));
    json_get_string(assets_start, assets_end, "placeholder_thumbnail",
                    ui->theme.placeholder_thumbnail,
                    sizeof(ui->theme.placeholder_thumbnail));
  }
  if (!ui->theme.font_fallback[0]) {
    copy_string(ui->theme.font_fallback, sizeof(ui->theme.font_fallback), "builtin");
  }

  if (json_find_object(json, json_end, "text_mode", &text_start, &text_end)) {
    ui->theme.force_no_icons = json_get_bool(text_start, text_end, "force_no_icons", 1);
    ui->theme.line_height = json_get_long(text_start, text_end, "line_height", 14);
    if (ui->theme.line_height <= 0 || ui->theme.line_height > 96) {
      ui->theme.line_height = 14;
      ui->theme.fallback = 1;
      copy_string(ui->theme.status, sizeof(ui->theme.status),
                  "invalid line_height; builtin text fallback");
    }
  }

  if (theme_behavior_is_blocked(json, json_end)) {
    ui->theme.fallback = 1;
    copy_string(ui->theme.status, sizeof(ui->theme.status),
                "theme requested behavior control; blocked");
  } else if (!ui->theme.status[0] || strcmp(ui->theme.status, "builtin text fallback") == 0) {
    copy_string(ui->theme.status, sizeof(ui->theme.status),
                ui->theme.font_ui[0] ? "theme loaded; text fallback available"
                                     : "theme loaded; builtin font fallback");
  }

  free(json);
  return 1;
}

static void add_setting_entry(struct ui_state *ui, const char *id, const char *name,
                              const char *value) {
  struct setting_entry *entry;
  if (ui->setting_count >= UI_MAX_SETTINGS) {
    return;
  }
  entry = &ui->setting_entries[ui->setting_count++];
  memset(entry, 0, sizeof(*entry));
  copy_string(entry->id, sizeof(entry->id), id);
  copy_string(entry->display_name, sizeof(entry->display_name), name);
  copy_string(entry->value, sizeof(entry->value), value && value[0] ? value : "-");
}

static void add_bool_setting_entry(struct ui_state *ui, const char *id, const char *name,
                                   int value) {
  add_setting_entry(ui, id, name, value ? "true" : "false");
}

static void add_device_settings_entries(struct ui_state *ui) {
  char value[256];
  const struct device_settings *device = &ui->device;

  add_setting_entry(ui, "a30_policy", "A30 Policy", "read-only inventory");
  add_setting_entry(ui, "a30_write_policy", "A30 Write Policy",
                    "defer writes until backend validation");
  add_setting_entry(ui, "a30_config", "A30 Config",
                    device->loaded ? ui->system_config_path : device->status);

  snprintf(value, sizeof(value), "vol=%ld mute=%ld bgm=%ld",
           device->volume, device->mute, device->bgm_volume);
  add_setting_entry(ui, "a30_volume", "A30 Volume", value);
  add_setting_entry(ui, "a30_volume_backend", "A30 Volume Backend",
                    device->volume_backend);

  snprintf(value, sizeof(value), "brightness=%ld lumination=%ld",
           device->brightness, device->lumination);
  add_setting_entry(ui, "a30_brightness", "A30 Brightness", value);
  snprintf(value, sizeof(value), "contrast=%ld hue=%ld saturation=%ld",
           device->contrast, device->hue, device->saturation);
  add_setting_entry(ui, "a30_display_color", "A30 Display Color", value);
  add_setting_entry(ui, "a30_brightness_backend", "A30 Brightness Backend",
                    device->brightness_backend);

  snprintf(value, sizeof(value), "enabled=%ld", device->wifi_enabled);
  add_setting_entry(ui, "a30_wifi_config", "A30 Wi-Fi Config", value);
  if (device->wpa_loaded) {
    snprintf(value, sizeof(value), "%.32s ip=%.48s rssi=%.16s speed=%.16s freq=%.16s",
             device->wifi_state[0] ? device->wifi_state : "-",
             device->wifi_ip[0] ? device->wifi_ip : "-",
             device->wifi_rssi[0] ? device->wifi_rssi : "-",
             device->wifi_linkspeed[0] ? device->wifi_linkspeed : "-",
             device->wifi_frequency[0] ? device->wifi_frequency : "-");
  } else {
    copy_string(value, sizeof(value), "no redacted runtime status");
  }
  add_setting_entry(ui, "a30_wifi_runtime", "A30 Wi-Fi Runtime", value);

  add_setting_entry(ui, "a30_keymap", "A30 Keymap", device->keymap);
  add_setting_entry(ui, "a30_input_event", "A30 Input Event", ui->input_event_path);
  add_setting_entry(ui, "a30_language", "A30 Language", device->language);
  add_setting_entry(ui, "a30_stock_theme", "A30 Stock Theme", device->stock_theme);
  snprintf(value, sizeof(value), "cpufreq=%ld", device->cpu_freq_mode);
  add_setting_entry(ui, "a30_cpu_mode", "A30 CPU Mode", value);
}

static int load_settings_entries(struct ui_state *ui) {
  struct frontend_settings settings;

  ui->setting_count = 0;
  ui->settings_cursor = 0;
  if (!load_settings(ui->settings_path, &settings)) {
    return 0;
  }
  load_theme_state(ui, settings.theme_id);
  add_setting_entry(ui, "ui_mode", "UI Mode", settings.ui_mode);
  add_setting_entry(ui, "top_mode", "TOP Mode", settings.top_mode);
  add_setting_entry(ui, "rom_mode", "ROM Mode", settings.rom_mode);
  add_bool_setting_entry(ui, "show_empty_systems", "Show Empty Systems",
                         settings.show_empty_systems);
  add_bool_setting_entry(ui, "show_favorites_on_top", "Favorites On TOP",
                         settings.show_favorites_on_top);
  add_setting_entry(ui, "boot_resume_mode", "Boot Resume Mode",
                    settings.boot_resume_mode);
  add_setting_entry(ui, "sort_systems", "Sort Systems", settings.sort_systems);
  add_setting_entry(ui, "sort_roms", "Sort ROMs", settings.sort_roms);
  add_setting_entry(ui, "rom_scan_policy", "ROM Scan Policy",
                    settings.rom_scan_policy);
  add_setting_entry(ui, "theme_id", "Theme", settings.theme_id);
  add_setting_entry(ui, "theme_name", "Theme Name", ui->theme.display_name);
  add_setting_entry(ui, "theme_status", "Theme Status", ui->theme.status);
  add_setting_entry(ui, "theme_layout", "Theme Layout", ui->theme.layout_preset);
  add_setting_entry(ui, "theme_font", "Theme Font",
                    ui->theme.font_ui[0] ? ui->theme.font_ui : ui->theme.font_fallback);
  add_bool_setting_entry(ui, "theme_force_no_icons", "Text Force No Icons",
                         ui->theme.force_no_icons);
  load_device_settings(ui);
  add_device_settings_entries(ui);
  return 1;
}

static int load_top_entries(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *start;
  const char *end;
  const char *cursor;
  struct frontend_settings settings;

  ui->top_count = 0;
  if ((ui->refresh || !file_exists(ui->top_cache_path)) &&
      !run_scanner(ui->plumos_root, ui->sdcard_root, NULL)) {
    copy_string(ui->status, sizeof(ui->status), "full scan failed or scanner is missing");
  }
  if (!load_settings(ui->settings_path, &settings)) {
    copy_string(ui->status, sizeof(ui->status), "settings read failed");
  }

  json = read_file(ui->top_cache_path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "systems", &start, &end)) {
    free(json);
    return 0;
  }

  cursor = start;
  while (ui->top_count < UI_MAX_TOP) {
    const char *obj_start;
    const char *obj_end;
    struct top_entry entry;

    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }
    memset(&entry, 0, sizeof(entry));
    json_get_string(obj_start, obj_end, "id", entry.id, sizeof(entry.id));
    json_get_string(obj_start, obj_end, "display_name", entry.display_name,
                    sizeof(entry.display_name));
    json_get_string(obj_start, obj_end, "default_launch_profile", entry.default_launch_profile,
                    sizeof(entry.default_launch_profile));
    entry.rom_count = json_get_long(obj_start, obj_end, "rom_count", 0);
    entry.pinned = json_get_bool(obj_start, obj_end, "pinned", 0);
    if (!entry.display_name[0]) {
      copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
    }
    if (ui->show_all || settings.show_empty_systems || entry.rom_count > 0 || entry.pinned) {
      ui->top_entries[ui->top_count++] = entry;
    }
  }
  free(json);

  if (settings.show_favorites_on_top && ui->top_count < UI_MAX_TOP) {
    struct top_entry fav;
    memset(&fav, 0, sizeof(fav));
    copy_string(fav.id, sizeof(fav.id), "favorites");
    copy_string(fav.display_name, sizeof(fav.display_name), "Favorites");
    copy_string(fav.default_launch_profile, sizeof(fav.default_launch_profile),
                "internal:favorites");
    fav.rom_count = count_json_array_objects(ui->favorites_path, "favorites");
    fav.pinned = 1;
    fav.virtual_entry = 1;
    ui->top_entries[ui->top_count++] = fav;
  }
  if (ui->top_cursor >= ui->top_count) {
    ui->top_cursor = ui->top_count ? ui->top_count - 1 : 0;
  }
  return 1;
}

static int load_start_menu_entries(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *menus_start;
  const char *menus_end;
  const char *menu_cursor;

  ui->menu_count = 0;
  ui->menu_cursor = 0;
  json = read_file(ui->menus_path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "menus", &menus_start, &menus_end)) {
    free(json);
    return 0;
  }

  menu_cursor = menus_start;
  while (ui->menu_count < UI_MAX_MENU) {
    const char *menu_start;
    const char *menu_end;
    const char *entries_start;
    const char *entries_end;
    const char *entry_cursor;
    char menu_id[64] = "";

    if (!json_next_object(&menu_cursor, menus_end, &menu_start, &menu_end)) {
      break;
    }
    json_get_string(menu_start, menu_end, "id", menu_id, sizeof(menu_id));
    if (strcmp(menu_id, "start") != 0) {
      continue;
    }
    if (!json_find_array(menu_start, menu_end, "entries", &entries_start, &entries_end)) {
      break;
    }
    entry_cursor = entries_start;
    while (ui->menu_count < UI_MAX_MENU) {
      const char *obj_start;
      const char *obj_end;
      struct menu_entry entry;

      if (!json_next_object(&entry_cursor, entries_end, &obj_start, &obj_end)) {
        break;
      }
      memset(&entry, 0, sizeof(entry));
      json_get_string(obj_start, obj_end, "id", entry.id, sizeof(entry.id));
      json_get_string(obj_start, obj_end, "display_name", entry.display_name,
                      sizeof(entry.display_name));
      json_get_string(obj_start, obj_end, "kind", entry.kind, sizeof(entry.kind));
      json_get_string(obj_start, obj_end, "action", entry.action, sizeof(entry.action));
      entry.confirm = json_get_bool(obj_start, obj_end, "confirm", 0);
      if (!entry.display_name[0]) {
        copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
      }
      ui->menu_entries[ui->menu_count++] = entry;
    }
    break;
  }
  free(json);
  return 1;
}

static int load_favorite_entries(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *start;
  const char *end;
  const char *cursor;

  ui->rom_count = 0;
  ui->rom_cursor = 0;
  json = read_file(ui->favorites_path, &json_size);
  if (!json) {
    return file_exists(ui->favorites_path) ? 0 : 1;
  }
  if (!json_find_array(json, json + json_size, "favorites", &start, &end)) {
    free(json);
    return 0;
  }
  cursor = start;
  while (ui->rom_count < UI_MAX_ROMS) {
    const char *obj_start;
    const char *obj_end;
    struct rom_entry entry;

    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }
    memset(&entry, 0, sizeof(entry));
    json_get_string(obj_start, obj_end, "system_id", entry.system_id,
                    sizeof(entry.system_id));
    json_get_string(obj_start, obj_end, "title", entry.title, sizeof(entry.title));
    json_get_string(obj_start, obj_end, "relative_path", entry.relative_path,
                    sizeof(entry.relative_path));
    json_get_string(obj_start, obj_end, "path", entry.path, sizeof(entry.path));
    if (!entry.title[0]) {
      copy_string(entry.title, sizeof(entry.title), entry.relative_path);
    }
    {
      size_t pos = 0;
      append_string(entry.detail, sizeof(entry.detail), &pos, entry.system_id);
      append_string(entry.detail, sizeof(entry.detail), &pos, " / ");
      append_string(entry.detail, sizeof(entry.detail), &pos, entry.relative_path);
    }
    ui->rom_entries[ui->rom_count++] = entry;
  }
  free(json);
  return 1;
}

static int load_recent_entries(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *start;
  const char *end;
  const char *cursor;

  ui->rom_count = 0;
  ui->rom_cursor = 0;
  json = read_file(ui->recent_path, &json_size);
  if (!json) {
    return file_exists(ui->recent_path) ? 0 : 1;
  }
  if (!json_find_array(json, json + json_size, "recents", &start, &end)) {
    free(json);
    return 0;
  }
  cursor = start;
  while (ui->rom_count < UI_MAX_ROMS) {
    const char *obj_start;
    const char *obj_end;
    struct rom_entry entry;
    char last_played_at[64] = "";

    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }
    memset(&entry, 0, sizeof(entry));
    json_get_string(obj_start, obj_end, "system_id", entry.system_id,
                    sizeof(entry.system_id));
    json_get_string(obj_start, obj_end, "title", entry.title, sizeof(entry.title));
    json_get_string(obj_start, obj_end, "relative_path", entry.relative_path,
                    sizeof(entry.relative_path));
    json_get_string(obj_start, obj_end, "path", entry.path, sizeof(entry.path));
    json_get_string(obj_start, obj_end, "launch_profile", entry.launch_profile,
                    sizeof(entry.launch_profile));
    json_get_string(obj_start, obj_end, "last_played_at", last_played_at,
                    sizeof(last_played_at));
    entry.resume_available = json_get_bool(obj_start, obj_end, "resume_available", 0);
    if (!entry.title[0]) {
      copy_string(entry.title, sizeof(entry.title), entry.relative_path);
    }
    {
      size_t pos = strlen(entry.detail);
      append_string(entry.detail, sizeof(entry.detail), &pos, entry.system_id);
      append_string(entry.detail, sizeof(entry.detail), &pos, " / resume=");
      append_string(entry.detail, sizeof(entry.detail), &pos,
                    entry.resume_available ? "yes" : "no");
      append_string(entry.detail, sizeof(entry.detail), &pos, " / ");
      append_string(entry.detail, sizeof(entry.detail), &pos,
                    entry.launch_profile[0] ? entry.launch_profile : "-");
    }
    if (last_played_at[0]) {
      size_t pos = strlen(entry.detail);
      append_string(entry.detail, sizeof(entry.detail), &pos, " / ");
      append_string(entry.detail, sizeof(entry.detail), &pos, last_played_at);
    }
    ui->rom_entries[ui->rom_count++] = entry;
  }
  free(json);
  return 1;
}

static int build_system_cache_path(char *out, size_t out_size, const char *plumos_root,
                                   const char *system_id) {
  char dir[PATH_MAX];
  char name[128];
  size_t id_len = strlen(system_id);

  if (id_len + 6 > sizeof(name)) {
    return 0;
  }
  memcpy(name, system_id, id_len);
  memcpy(name + id_len, ".json", 6);
  if (!join_path(dir, sizeof(dir), plumos_root, "state/frontend/systems")) {
    return 0;
  }
  return join_path(out, out_size, dir, name);
}

static int load_rom_entries(struct ui_state *ui, const char *system_id) {
  char path[PATH_MAX];
  char *json;
  size_t json_size;
  const char *start;
  const char *end;
  const char *cursor;

  ui->rom_count = 0;
  ui->rom_cursor = 0;
  if (strcmp(system_id, "favorites") == 0) {
    return load_favorite_entries(ui);
  }
  if (!build_system_cache_path(path, sizeof(path), ui->plumos_root, system_id)) {
    return 0;
  }
  if (!run_scanner(ui->plumos_root, ui->sdcard_root, system_id) && !file_exists(path)) {
    return 0;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "roms", &start, &end)) {
    free(json);
    return 0;
  }

  cursor = start;
  while (ui->rom_count < UI_MAX_ROMS) {
    const char *obj_start;
    const char *obj_end;
    struct rom_entry entry;

    if (!json_next_object(&cursor, end, &obj_start, &obj_end)) {
      break;
    }
    memset(&entry, 0, sizeof(entry));
    copy_string(entry.system_id, sizeof(entry.system_id), system_id);
    json_get_string(obj_start, obj_end, "title", entry.title, sizeof(entry.title));
    json_get_string(obj_start, obj_end, "relative_path", entry.relative_path,
                    sizeof(entry.relative_path));
    json_get_string(obj_start, obj_end, "path", entry.path, sizeof(entry.path));
    if (!entry.title[0]) {
      copy_string(entry.title, sizeof(entry.title), entry.relative_path);
    }
    ui->rom_entries[ui->rom_count++] = entry;
  }
  free(json);
  return 1;
}

static void clear_screen(const struct ui_state *ui) {
  if (!ui->no_clear) {
    printf("\033[2J\033[H");
  }
}

static void render_top(const struct ui_state *ui) {
  size_t i;
  size_t window = 10;
  size_t start = 0;
  size_t end;

  if (ui->top_cursor >= window / 2) {
    start = ui->top_cursor - window / 2;
  }
  if (start + window > ui->top_count) {
    start = ui->top_count > window ? ui->top_count - window : 0;
  }
  end = start + window;
  if (end > ui->top_count) {
    end = ui->top_count;
  }

  printf("plumOS controller UI - TOP\n");
  printf("A: open  B: back  START: menu  SELECT: core preview  FUNCTION: safe menu  Q: quit\n");
  printf("entries=%zu cursor=%zu\n", ui->top_count, ui->top_count ? ui->top_cursor + 1 : 0);
  printf("\n");
  for (i = start; i < end; i++) {
    const struct top_entry *entry = &ui->top_entries[i];
    printf("%c %3zu  %-18s ROMs=%-5ld profile=%s\n",
           i == ui->top_cursor ? '>' : ' ', i + 1, entry->display_name, entry->rom_count,
           entry->default_launch_profile[0] ? entry->default_launch_profile : "-");
  }
  if (ui->top_count == 0) {
    printf("(system entry is empty; run plumos-library-scan first)\n");
  }
  if (ui->status[0]) {
    printf("\nstatus: %s\n", ui->status);
  }
}

static void render_roms(const struct ui_state *ui) {
  size_t i;
  size_t window = 10;
  size_t start = 0;
  size_t end;
  const char *title = "ROMS";
  const char *subtitle =
      "A: launch preview  B/LEFT: TOP  START: menu  SELECT: core preview  FUNCTION: safe menu  Q: quit";

  if (ui->rom_cursor >= window / 2) {
    start = ui->rom_cursor - window / 2;
  }
  if (start + window > ui->rom_count) {
    start = ui->rom_count > window ? ui->rom_count - window : 0;
  }
  end = start + window;
  if (end > ui->rom_count) {
    end = ui->rom_count;
  }

  if (ui->screen == SCREEN_FAVORITES) {
    title = "FAVORITES";
    subtitle =
        "A: launch preview  B/LEFT: TOP  START: menu  SELECT: core preview  FUNCTION: safe menu  Q: quit";
  } else if (ui->screen == SCREEN_RECENT) {
    title = "RECENT";
    subtitle =
        "A: resume preview  B/LEFT: TOP  START: menu  SELECT: core preview  FUNCTION: safe menu  Q: quit";
  }

  printf("plumOS controller UI - %s\n", title);
  if (ui->screen == SCREEN_ROMS) {
    printf("system=%s (%s)\n", ui->current_system_id, ui->current_system_name);
  }
  printf("%s\n", subtitle);
  printf("entries=%zu cursor=%zu\n", ui->rom_count, ui->rom_count ? ui->rom_cursor + 1 : 0);
  printf("\n");
  for (i = start; i < end; i++) {
    const struct rom_entry *entry = &ui->rom_entries[i];
    const char *detail = entry->detail[0] ? entry->detail : entry->relative_path;
    printf("%c %3zu  %-30s %s\n",
           i == ui->rom_cursor ? '>' : ' ', i + 1, entry->title, detail);
  }
  if (ui->rom_count == 0) {
    printf("(entry is empty)\n");
  }
  if (ui->screen == SCREEN_FAVORITES) {
    printf("\nsource: %s\n", ui->favorites_path);
  } else if (ui->screen == SCREEN_RECENT) {
    printf("\nsource: %s\n", ui->recent_path);
  }
  if (ui->status[0]) {
    printf("\nstatus: %s\n", ui->status);
  }
}

static void render_start_menu(const struct ui_state *ui) {
  size_t i;

  printf("plumOS controller UI - START\n");
  printf("A: open/preview  B/LEFT: back  UP/DOWN: move  Q: quit\n");
  printf("entries=%zu cursor=%zu\n", ui->menu_count, ui->menu_count ? ui->menu_cursor + 1 : 0);
  printf("\n");
  for (i = 0; i < ui->menu_count; i++) {
    const struct menu_entry *entry = &ui->menu_entries[i];
    printf("%c %3zu  %-24s %-10s %-24s %s\n",
           i == ui->menu_cursor ? '>' : ' ', i + 1, entry->display_name,
           entry->kind[0] ? entry->kind : "-", entry->action,
           entry->confirm ? "confirm" : "");
  }
  if (ui->menu_count == 0) {
    printf("(menu entry is empty)\n");
  }
  if (ui->status[0]) {
    printf("\nstatus: %s\n", ui->status);
  }
}

static void render_settings(const struct ui_state *ui) {
  size_t i;

  printf("plumOS controller UI - SETTINGS\n");
  printf("A: edit preview  B/LEFT: back  UP/DOWN: move  Q: quit\n");
  printf("entries=%zu cursor=%zu\n", ui->setting_count,
         ui->setting_count ? ui->settings_cursor + 1 : 0);
  printf("\n");
  for (i = 0; i < ui->setting_count; i++) {
    const struct setting_entry *entry = &ui->setting_entries[i];
    printf("%c %3zu  %-24s %s\n",
           i == ui->settings_cursor ? '>' : ' ', i + 1, entry->display_name, entry->value);
  }
  printf("\nsource: %s\n", ui->settings_path);
  if (ui->status[0]) {
    printf("\nstatus: %s\n", ui->status);
  }
}

static void render_ui(const struct ui_state *ui) {
  clear_screen(ui);
  if (ui->screen == SCREEN_ROMS || ui->screen == SCREEN_FAVORITES ||
      ui->screen == SCREEN_RECENT) {
    render_roms(ui);
  } else if (ui->screen == SCREEN_START_MENU) {
    render_start_menu(ui);
  } else if (ui->screen == SCREEN_SETTINGS) {
    render_settings(ui);
  } else {
    render_top(ui);
  }
  fflush(stdout);
}

static void set_status(struct ui_state *ui, const char *text) {
  copy_string(ui->status, sizeof(ui->status), text);
}

static void open_start_menu(struct ui_state *ui) {
  ui->back_screen = ui->screen;
  ui->screen = SCREEN_START_MENU;
  if (!load_start_menu_entries(ui)) {
    set_status(ui, "cannot load START menu");
  } else {
    set_status(ui, "START menu ready");
  }
}

static void open_favorites_screen(struct ui_state *ui) {
  ui->screen = SCREEN_FAVORITES;
  copy_string(ui->current_system_id, sizeof(ui->current_system_id), "favorites");
  copy_string(ui->current_system_name, sizeof(ui->current_system_name), "Favorites");
  if (!load_favorite_entries(ui)) {
    set_status(ui, "cannot load Favorites");
  } else {
    set_status(ui, "Favorites ready");
  }
}

static void open_recent_screen(struct ui_state *ui) {
  ui->screen = SCREEN_RECENT;
  copy_string(ui->current_system_id, sizeof(ui->current_system_id), "recent");
  copy_string(ui->current_system_name, sizeof(ui->current_system_name), "Recent");
  if (!load_recent_entries(ui)) {
    set_status(ui, "cannot load Recent");
  } else {
    set_status(ui, "Recent ready");
  }
}

static void open_settings_screen(struct ui_state *ui) {
  ui->screen = SCREEN_SETTINGS;
  if (!load_settings_entries(ui)) {
    set_status(ui, "cannot load Settings");
  } else {
    set_status(ui, "Settings ready");
  }
}

static void open_rom_screen(struct ui_state *ui, const struct top_entry *entry) {
  copy_string(ui->current_system_id, sizeof(ui->current_system_id), entry->id);
  copy_string(ui->current_system_name, sizeof(ui->current_system_name), entry->display_name);
  if (strcmp(entry->id, "favorites") == 0) {
    open_favorites_screen(ui);
    return;
  }
  ui->screen = SCREEN_ROMS;
  set_status(ui, "on-enter ROM scan");
  if (!load_rom_entries(ui, entry->id)) {
    set_status(ui, "cannot load ROM list");
  } else if (ui->rom_count == UI_MAX_ROMS) {
    set_status(ui, "ROM list truncated at prototype limit");
  } else {
    set_status(ui, "ROM list ready");
  }
}

static void handle_action(struct ui_state *ui, enum ui_action action) {
  if (action == ACTION_NONE) {
    return;
  }
  if (action == ACTION_QUIT) {
    set_status(ui, "quit");
    return;
  }
  if (action == ACTION_FUNCTION) {
    set_status(ui, "safe shutdown/resume menu preview: sleep shutdown cancel");
    return;
  }

  if (ui->screen == SCREEN_TOP) {
    if (action == ACTION_UP) {
      if (ui->top_cursor > 0) {
        ui->top_cursor--;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->top_cursor + 1 < ui->top_count) {
        ui->top_cursor++;
      }
      return;
    }
    if ((action == ACTION_A || action == ACTION_RIGHT) && ui->top_count > 0) {
      const struct top_entry *entry = &ui->top_entries[ui->top_cursor];
      open_rom_screen(ui, entry);
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    if (action == ACTION_SELECT && ui->top_count > 0) {
      const struct top_entry *entry = &ui->top_entries[ui->top_cursor];
      char msg[256];
      snprintf(msg, sizeof(msg), "system core preview: %s profile=%s", entry->id,
               entry->default_launch_profile[0] ? entry->default_launch_profile : "-");
      set_status(ui, msg);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_START_MENU) {
    if (action == ACTION_UP) {
      if (ui->menu_cursor > 0) {
        ui->menu_cursor--;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->menu_cursor + 1 < ui->menu_count) {
        ui->menu_cursor++;
      }
      return;
    }
    if (action == ACTION_B || action == ACTION_LEFT) {
      ui->screen = ui->back_screen;
      set_status(ui, "close START menu");
      return;
    }
    if (action == ACTION_A || action == ACTION_RIGHT) {
      const struct menu_entry *entry;
      if (ui->menu_count == 0) {
        return;
      }
      entry = &ui->menu_entries[ui->menu_cursor];
      if (strcmp(entry->action, "internal:settings") == 0) {
        open_settings_screen(ui);
      } else if (strcmp(entry->action, "internal:favorites") == 0) {
        open_favorites_screen(ui);
      } else if (strcmp(entry->action, "internal:recent") == 0) {
        open_recent_screen(ui);
      } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "menu preview: %s action=%s", entry->display_name,
                 entry->action);
        set_status(ui, msg);
      }
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_SETTINGS) {
    if (action == ACTION_UP) {
      if (ui->settings_cursor > 0) {
        ui->settings_cursor--;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->settings_cursor + 1 < ui->setting_count) {
        ui->settings_cursor++;
      }
      return;
    }
    if (action == ACTION_B || action == ACTION_LEFT) {
      ui->screen = SCREEN_TOP;
      set_status(ui, "back to TOP");
      return;
    }
    if (action == ACTION_A && ui->setting_count > 0) {
      const struct setting_entry *entry = &ui->setting_entries[ui->settings_cursor];
      char msg[256];
      snprintf(msg, sizeof(msg), "settings edit preview: %s=%s", entry->id, entry->value);
      set_status(ui, msg);
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (action == ACTION_UP) {
    if (ui->rom_cursor > 0) {
      ui->rom_cursor--;
    }
    return;
  }
  if (action == ACTION_DOWN) {
    if (ui->rom_cursor + 1 < ui->rom_count) {
      ui->rom_cursor++;
    }
    return;
  }
  if (action == ACTION_B || action == ACTION_LEFT) {
    ui->screen = SCREEN_TOP;
    set_status(ui, "back to TOP");
    return;
  }
  if (action == ACTION_A && ui->rom_count > 0) {
    const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
    char msg[256];
    snprintf(msg, sizeof(msg), "%s preview: %s / %s",
             ui->screen == SCREEN_RECENT ? "resume" : "launch",
             entry->system_id[0] ? entry->system_id : ui->current_system_id,
             entry->relative_path);
    set_status(ui, msg);
    return;
  }
  if (action == ACTION_START) {
    open_start_menu(ui);
    return;
  }
  if (action == ACTION_SELECT && ui->rom_count > 0) {
    const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
    char msg[256];
    snprintf(msg, sizeof(msg), "per-ROM core preview: %s / %s",
             entry->system_id[0] ? entry->system_id : ui->current_system_id,
             entry->relative_path);
    set_status(ui, msg);
    return;
  }
}

static enum ui_action action_from_key_code(unsigned int code) {
  switch (code) {
  case KEY_UP:
    return ACTION_UP;
  case KEY_DOWN:
    return ACTION_DOWN;
  case KEY_LEFT:
    return ACTION_LEFT;
  case KEY_RIGHT:
    return ACTION_RIGHT;
  case KEY_SPACE:
  case BTN_SOUTH:
  case KEY_Z:
  case 7:
    return ACTION_A;
  case KEY_LEFTCTRL:
  case BTN_EAST:
  case KEY_X:
  case 9:
    return ACTION_B;
  case KEY_ENTER:
  case KEY_MENU:
  case BTN_START:
  case BTN_MODE:
  case KEY_HOME:
  case 10:
    return ACTION_START;
  case KEY_RIGHTCTRL:
  case KEY_SELECT:
  case BTN_SELECT:
    return ACTION_SELECT;
  case KEY_ESC:
    return ACTION_FUNCTION;
  case KEY_Q:
    return ACTION_QUIT;
  default:
    return ACTION_NONE;
  }
}

static enum ui_action action_from_stdin_char(int ch) {
  switch (ch) {
  case 'w':
  case 'W':
    return ACTION_UP;
  case 's':
  case 'S':
    return ACTION_DOWN;
  case 'a':
  case 'A':
    return ACTION_LEFT;
  case 'd':
  case 'D':
    return ACTION_RIGHT;
  case 'e':
  case 'E':
  case '\n':
  case ' ':
    return ACTION_A;
  case 'b':
  case 'B':
    return ACTION_B;
  case 'm':
  case 'M':
    return ACTION_START;
  case 'c':
  case 'C':
    return ACTION_SELECT;
  case 'f':
  case 'F':
    return ACTION_FUNCTION;
  case 'q':
  case 'Q':
    return ACTION_QUIT;
  default:
    return ACTION_NONE;
  }
}

static enum ui_action action_from_script_token(const char *token) {
  if (strcmp(token, "up") == 0) {
    return ACTION_UP;
  }
  if (strcmp(token, "down") == 0) {
    return ACTION_DOWN;
  }
  if (strcmp(token, "left") == 0) {
    return ACTION_LEFT;
  }
  if (strcmp(token, "right") == 0) {
    return ACTION_RIGHT;
  }
  if (strcmp(token, "a") == 0 || strcmp(token, "open") == 0) {
    return ACTION_A;
  }
  if (strcmp(token, "b") == 0 || strcmp(token, "back") == 0) {
    return ACTION_B;
  }
  if (strcmp(token, "start") == 0) {
    return ACTION_START;
  }
  if (strcmp(token, "select") == 0) {
    return ACTION_SELECT;
  }
  if (strcmp(token, "function") == 0 || strcmp(token, "safe") == 0) {
    return ACTION_FUNCTION;
  }
  if (strcmp(token, "q") == 0 || strcmp(token, "quit") == 0) {
    return ACTION_QUIT;
  }
  return ACTION_NONE;
}

static int discover_input_event(char *out, size_t out_size) {
  FILE *f;
  char line[512];
  int in_gpio = 0;

  f = fopen("/proc/bus/input/devices", "rb");
  if (!f) {
    return copy_string(out, out_size, "/dev/input/event3");
  }
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '\n') {
      in_gpio = 0;
      continue;
    }
    if (strstr(line, "Name=\"gpio-keys-polled\"")) {
      in_gpio = 1;
      continue;
    }
    if (in_gpio && strstr(line, "Handlers=")) {
      char *event = strstr(line, "event");
      if (event) {
        char name[32];
        size_t n = 0;
        while (event[n] && !isspace((unsigned char)event[n]) && n + 1 < sizeof(name)) {
          name[n] = event[n];
          n++;
        }
        name[n] = '\0';
        fclose(f);
        return join_path(out, out_size, "/dev/input", name);
      }
    }
  }
  fclose(f);
  return copy_string(out, out_size, "/dev/input/event3");
}

static void dump_input_events(const char *event_path, int timeout_sec) {
  int fd;
  time_t deadline;

  fd = open(event_path, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    fprintf(stderr, "error: cannot open input event device: %s: %s\n", event_path,
            strerror(errno));
    return;
  }
  deadline = time(NULL) + timeout_sec;
  printf("plumOS controller UI - dump events\n");
  printf("event: %s timeout=%d\n", event_path, timeout_sec);
  fflush(stdout);

  while (time(NULL) < deadline) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 250) > 0 && (pfd.revents & POLLIN)) {
      struct input_event ev;
      while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        printf("type=%u code=%u value=%d\n", ev.type, ev.code, ev.value);
        fflush(stdout);
      }
    }
  }
  close(fd);
}

static int run_script(struct ui_state *ui, const char *script) {
  char *copy;
  char *token;
  char *save = NULL;

  copy = strdup(script);
  if (!copy) {
    return 0;
  }
  render_ui(ui);
  for (token = strtok_r(copy, ",", &save); token; token = strtok_r(NULL, ",", &save)) {
    enum ui_action action;
    while (*token && isspace((unsigned char)*token)) {
      token++;
    }
    action = action_from_script_token(token);
    handle_action(ui, action);
    render_ui(ui);
    if (action == ACTION_QUIT) {
      break;
    }
  }
  free(copy);
  return 1;
}

static int run_event_loop(struct ui_state *ui, const char *event_path) {
  int event_fd = -1;
  int stdin_fd = STDIN_FILENO;
  int old_flags = -1;
  time_t deadline = 0;

  if (event_path && event_path[0]) {
    event_fd = open(event_path, O_RDONLY | O_NONBLOCK);
    if (event_fd < 0) {
      snprintf(ui->status, sizeof(ui->status), "input open failed: %s", strerror(errno));
    }
  }
  old_flags = fcntl(stdin_fd, F_GETFL, 0);
  if (old_flags >= 0) {
    fcntl(stdin_fd, F_SETFL, old_flags | O_NONBLOCK);
  }
  if (ui->timeout_sec > 0) {
    deadline = time(NULL) + ui->timeout_sec;
  }

  render_ui(ui);
  while (1) {
    struct pollfd pfds[2];
    nfds_t count = 0;
    int rc;
    enum ui_action action = ACTION_NONE;

    if (event_fd >= 0) {
      pfds[count].fd = event_fd;
      pfds[count].events = POLLIN;
      pfds[count].revents = 0;
      count++;
    }
    pfds[count].fd = stdin_fd;
    pfds[count].events = POLLIN;
    pfds[count].revents = 0;
    count++;

    if (ui->timeout_sec > 0 && time(NULL) >= deadline) {
      break;
    }
    rc = poll(pfds, count, 250);
    if (rc < 0 && errno != EINTR) {
      break;
    }
    if (rc <= 0) {
      continue;
    }

    if (event_fd >= 0 && (pfds[0].revents & POLLIN)) {
      struct input_event ev;
      while (read(event_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_KEY && (ev.value == 1 || ev.value == 2)) {
          action = action_from_key_code(ev.code);
          if (action == ACTION_NONE) {
            snprintf(ui->status, sizeof(ui->status), "unmapped key code=%u value=%d", ev.code,
                     ev.value);
          }
        }
      }
    }
    if (pfds[count - 1].revents & POLLIN) {
      char buf[32];
      ssize_t n = read(stdin_fd, buf, sizeof(buf));
      ssize_t i;
      for (i = 0; i < n; i++) {
        enum ui_action stdin_action = action_from_stdin_char((unsigned char)buf[i]);
        if (stdin_action != ACTION_NONE) {
          action = stdin_action;
        }
      }
    }

    if (action != ACTION_NONE) {
      handle_action(ui, action);
      render_ui(ui);
      if (action == ACTION_QUIT) {
        break;
      }
    }
  }
  if (old_flags >= 0) {
    fcntl(stdin_fd, F_SETFL, old_flags);
  }
  if (event_fd >= 0) {
    close(event_fd);
  }
  return 0;
}

static void usage(const char *argv0) {
  printf("Usage:\n");
  printf("  %s [--all] [--refresh] [--once] [--timeout SEC] [--event PATH]\n", argv0);
  printf("  %s --script up,down,a,b,select,start,q [--no-clear]\n", argv0);
  printf("  %s --dump-events [--timeout SEC] [--event PATH]\n", argv0);
  printf("\n");
  printf("Keyboard fallback over SSH: w/s/a/d, e or space for A, b, m, c, f, q.\n");
  printf("Environment:\n");
  printf("  PLUMOS_SDCARD_ROOT  Default: /mnt/SDCARD\n");
  printf("  PLUMOS_ROOT         Default: $PLUMOS_SDCARD_ROOT/plumos\n");
  printf("  PLUMOS_INPUT_EVENT  Default: auto-detect gpio-keys-polled\n");
  printf("  PLUMOS_A30_SYSTEM_JSON  Default: /config/system.json\n");
  printf("  PLUMOS_A30_WPA_STATUS   Default: /tmp/wpa_status.txt\n");
}

int main(int argc, char **argv) {
  static struct ui_state ui;
  const char *plumos_root_env;
  const char *system_config_env;
  const char *wpa_status_env;
  const char *script = NULL;
  char event_path[PATH_MAX];
  int dump_events = 0;
  int i;

  memset(&ui, 0, sizeof(ui));
  ui.sdcard_root = getenv("PLUMOS_SDCARD_ROOT");
  if (!ui.sdcard_root || !ui.sdcard_root[0]) {
    ui.sdcard_root = "/mnt/SDCARD";
  }
  plumos_root_env = getenv("PLUMOS_ROOT");
  if (plumos_root_env && plumos_root_env[0]) {
    copy_string(ui.plumos_root, sizeof(ui.plumos_root), plumos_root_env);
  } else if (!join_path(ui.plumos_root, sizeof(ui.plumos_root), ui.sdcard_root, "plumos")) {
    fprintf(stderr, "error: plumOS root path is too long\n");
    return 1;
  }
  ui.timeout_sec = 0;
  discover_input_event(event_path, sizeof(event_path));
  if (getenv("PLUMOS_INPUT_EVENT") && getenv("PLUMOS_INPUT_EVENT")[0]) {
    copy_string(event_path, sizeof(event_path), getenv("PLUMOS_INPUT_EVENT"));
  }
  system_config_env = getenv("PLUMOS_A30_SYSTEM_JSON");
  if (!copy_string(ui.system_config_path, sizeof(ui.system_config_path),
                   system_config_env && system_config_env[0] ? system_config_env
                                                             : "/config/system.json")) {
    fprintf(stderr, "error: A30 system config path is too long\n");
    return 1;
  }
  wpa_status_env = getenv("PLUMOS_A30_WPA_STATUS");
  if (!copy_string(ui.wpa_status_path, sizeof(ui.wpa_status_path),
                   wpa_status_env && wpa_status_env[0] ? wpa_status_env
                                                       : "/tmp/wpa_status.txt")) {
    fprintf(stderr, "error: A30 WPA status path is too long\n");
    return 1;
  }

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--all") == 0) {
      ui.show_all = 1;
    } else if (strcmp(argv[i], "--refresh") == 0) {
      ui.refresh = 1;
    } else if (strcmp(argv[i], "--once") == 0) {
      ui.once = 1;
    } else if (strcmp(argv[i], "--no-clear") == 0) {
      ui.no_clear = 1;
    } else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
      ui.timeout_sec = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
      copy_string(event_path, sizeof(event_path), argv[++i]);
    } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
      script = argv[++i];
    } else if (strcmp(argv[i], "--dump-events") == 0) {
      dump_events = 1;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "error: unknown option: %s\n", argv[i]);
      usage(argv[0]);
      return 2;
    }
  }
  copy_string(ui.input_event_path, sizeof(ui.input_event_path), event_path);

  if (dump_events) {
    dump_input_events(event_path, ui.timeout_sec > 0 ? ui.timeout_sec : 10);
    return 0;
  }

  if (!join_path(ui.top_cache_path, sizeof(ui.top_cache_path), ui.plumos_root,
                 "state/frontend/library-index.json") ||
      !join_path(ui.settings_path, sizeof(ui.settings_path), ui.plumos_root,
                 "config/frontend/settings.json") ||
      !join_path(ui.menus_path, sizeof(ui.menus_path), ui.plumos_root,
                 "config/frontend/menus.json") ||
      !join_path(ui.favorites_path, sizeof(ui.favorites_path), ui.plumos_root,
                 "state/frontend/favorites.json") ||
      !join_path(ui.recent_path, sizeof(ui.recent_path), ui.plumos_root,
                 "state/frontend/recent.json")) {
    fprintf(stderr, "error: frontend path is too long\n");
    return 1;
  }

  if (!load_top_entries(&ui)) {
    fprintf(stderr, "error: cannot load TOP entries: %s\n", ui.top_cache_path);
    return 1;
  }
  if (script) {
    return run_script(&ui, script) ? 0 : 1;
  }
  if (ui.once) {
    render_ui(&ui);
    return 0;
  }
  return run_event_loop(&ui, event_path);
}
