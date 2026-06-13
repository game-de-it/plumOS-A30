#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef PLUMOS_ENABLE_MALI_RENDERER
#include "plumos_mali_renderer.h"
#endif

#ifndef PLUMOS_MALI_SETTING_FLASH_MARKER
#define PLUMOS_MALI_SETTING_FLASH_MARKER "@{F:"
#endif

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
#define UI_MAX_SCRAPING_CHOICES 64
#define UI_MAX_CORE_PROFILES 16
#define UI_COMMAND_MAX 8192
#define UI_PATH_MAX 1024
#define UI_RENDER_MAX_LINES 64
#define UI_RENDER_LINE_MAX 512
#define UI_THUMBNAIL_RESULT_MAX_LINES 256
#define UI_KEY_REPEAT_DELAY_MS 350
#define UI_KEY_REPEAT_INTERVAL_MS 95
#define UI_SETTING_VALUE_REPEAT_INTERVAL_MS 250
#define UI_MAX_WIFI_NETWORKS 64
#define UI_WIFI_SSID_MAX 160
#define UI_WIFI_PASSWORD_MAX 64
#define UI_WIFI_KEYBOARD_ROWS 7
#define UI_WIFI_COMMAND_ROW 6
#define UI_WIFI_COMMAND_COUNT 5
#define UI_USB_DISK_START_DELAY_MS 2000
#define UI_THUMBNAIL_RESULT_WINDOW 11
#define UI_SCRAPING_FIELD_COUNT 3
#define UI_SCRAPING_FIELD_IMAGE 0
#define UI_SCRAPING_FIELD_EXISTING 1
#define UI_SCRAPING_FIELD_SYSTEM 2
#define UI_GALLERY_TRANSITION_MS 360
#define UI_SDCARD_CLEANUP_MIN_INTERVAL_MS 60000
#define UI_ROM_SCAN_REFRESH_MIN_INTERVAL_MS 3000
#define UI_FE_READY_FLAG_PATH "/tmp/plumos-fe-ready"
#define A30_LCD_BACKLIGHT_PATH "/sys/devices/virtual/disp/disp/attr/lcdbl"
#define A30_DISPLAY_ENHANCE_PATH "/sys/devices/virtual/disp/disp/attr/enhance"

struct top_entry {
  char id[64];
  char display_name[128];
  char default_launch_profile[128];
  long rom_count;
  int pinned;
  int virtual_entry;
};

struct scraping_choice {
  char id[64];
  char display_name[128];
  long rom_count;
};

struct scraping_kind_choice {
  const char *display_name;
  const char *scraper_kind;
};

static const struct scraping_kind_choice SCRAPING_KIND_CHOICES[] = {
    {"Box Art", "Named_Boxarts"},
    {"Title Screen", "Named_Titles"},
};

struct core_profile_choice {
  char id[128];
};

struct rom_entry {
  char system_id[64];
  char title[256];
  char relative_path[UI_PATH_MAX];
  char path[UI_PATH_MAX];
  char thumbnail[UI_PATH_MAX];
  char launch_profile[128];
  char detail[256];
  int resume_available;
};

static int dirname_path(char *out, size_t out_size, const char *path);
static long clamp_long(long value, long min_value, long max_value);

static int rom_entry_alias_root_path(const struct rom_entry *entry, char *out,
                                     size_t out_size) {
  const char *slash;
  size_t alias_len;
  size_t rel_len;
  size_t path_len;
  size_t prefix_len;

  if (!entry || !out || out_size == 0 || !entry->path[0]) {
    return 0;
  }
  out[0] = '\0';
  if (!entry->relative_path[0]) {
    return dirname_path(out, out_size, entry->path);
  }
  slash = strchr(entry->relative_path, '/');
  if (!slash || slash == entry->relative_path) {
    return dirname_path(out, out_size, entry->path);
  }
  alias_len = (size_t)(slash - entry->relative_path);
  rel_len = strlen(entry->relative_path);
  path_len = strlen(entry->path);
  if (path_len < rel_len ||
      strcmp(entry->path + path_len - rel_len, entry->relative_path) != 0) {
    return dirname_path(out, out_size, entry->path);
  }
  prefix_len = path_len - rel_len;
  if (prefix_len + alias_len + 1 > out_size) {
    return 0;
  }
  memcpy(out, entry->path, prefix_len);
  memcpy(out + prefix_len, entry->relative_path, alias_len);
  out[prefix_len + alias_len] = '\0';
  return 1;
}

struct menu_entry {
  char id[64];
  char display_name[128];
  char kind[64];
  char action[256];
  int confirm;
  int background;
  int show_results;
};

struct setting_entry {
  char id[64];
  char display_name[128];
  char value[256];
};

struct setting_choice {
  const char *raw;
  const char *display;
};

#define UI_GRAPHIC_THEME_CHOICE_MAX 32

struct graphic_theme_choice {
  char raw[64];
  char display[128];
};

struct performance_cpu_preset {
  const char *label;
  const char *policy;
  long freq_khz;
};

struct safe_entry {
  const char *id;
  const char *display_name;
  const char *detail;
};

struct wifi_network_entry {
  char ssid[UI_WIFI_SSID_MAX];
  char security[24];
  char signal[24];
};

struct theme_state {
  int loaded;
  int fallback;
  char id[64];
  char target[32];
  char display_name[128];
  char path[PATH_MAX];
  char layout_preset[64];
  char font_ui[UI_PATH_MAX];
  char font_fallback[64];
  char background[UI_PATH_MAX];
  char gallery_background[UI_PATH_MAX];
  char system_logo_root[UI_PATH_MAX];
  char placeholder_thumbnail[UI_PATH_MAX];
  char graphic_top_layout[32];
  char graphic_transition[32];
  char graphic_transition_axis[32];
  char graphic_transition_easing[32];
  long graphic_transition_ms;
  char color_background[16];
  char color_foreground[16];
  char color_muted[16];
  char color_accent[16];
  char color_panel[16];
  char color_panel_inner[16];
  char color_media_panel[16];
  char color_selection_background[16];
  char color_selection_foreground[16];
  char color_danger[16];
  char status[256];
};

struct device_settings {
  int loaded;
  int wpa_loaded;
  int wifi_runtime_enabled;
  long volume;
  long brightness;
  long lumination;
  long contrast;
  long hue;
  long saturation;
  char language[64];
  char theme[UI_PATH_MAX];
  char timezone[64];
  char model[64];
  char kernel_version[128];
  char sdcard_storage[128];
  char network_status_source[128];
  char network_control_status[128];
  char ssh_status[128];
  int ssh_service_running;
  int ftp_service_running;
  int sftp_service_running;
  int samba_service_running;
  char ftp_status[128];
  char sftp_status[128];
  char samba_status[128];
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
  int show_recent_on_top;
  char boot_resume_mode[32];
  char ui_mode[32];
  char top_mode[32];
  char rom_mode[32];
  char theme_id[64];
  char graphic_theme_id[64];
  char graphic_top_layout[32];
  char graphic_transition[32];
  char graphic_transition_axis[32];
  char graphic_transition_easing[32];
  long graphic_transition_ms;
  char sort_systems[64];
  char sort_roms[64];
  char rom_scan_policy[64];
  long rom_scan_slow_threshold_ms;
  long rom_scan_test_file_count;
  char last_system_id[64];
};

enum ui_screen {
  SCREEN_TOP = 0,
  SCREEN_ROMS = 1,
  SCREEN_START_MENU = 2,
  SCREEN_FAVORITES = 3,
  SCREEN_RECENT = 4,
  SCREEN_SETTINGS = 5,
  SCREEN_SAFE_MENU = 6,
  SCREEN_HELP = 7,
  SCREEN_CORE_SELECT = 8,
  SCREEN_NETWORK_RESCUE = 9,
  SCREEN_WIFI_CONNECT = 10,
  SCREEN_USB_DISK_CONFIRM = 11,
  SCREEN_USB_DISK_STARTING = 12,
  SCREEN_THUMBNAIL_RESULTS = 13,
  SCREEN_THUMBNAIL_RUNNING = 14,
  SCREEN_SCRAPING = 15,
  SCREEN_GALLERY = 16
};

enum wifi_connect_stage {
  WIFI_CONNECT_SELECT = 0,
  WIFI_CONNECT_PASSWORD,
  WIFI_CONNECT_RESULT
};

enum settings_category {
  SETTINGS_CATEGORY_UI = 0,
  SETTINGS_CATEGORY_UI_THEME,
  SETTINGS_CATEGORY_SYSTEM,
  SETTINGS_CATEGORY_SYSTEM_DISPLAY_COLOR,
  SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST,
  SETTINGS_CATEGORY_SYSTEM_TIME,
  SETTINGS_CATEGORY_SYSTEM_TIME_MANUAL,
  SETTINGS_CATEGORY_SYSTEM_INFORMATION,
  SETTINGS_CATEGORY_NETWORK,
  SETTINGS_CATEGORY_NETWORK_SERVICE,
  SETTINGS_CATEGORY_NETWORK_INFORMATION,
  SETTINGS_CATEGORY_PERFORMANCE
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
  ACTION_X,
  ACTION_FUNCTION,
  ACTION_VOLUME_DOWN,
  ACTION_VOLUME_UP,
  ACTION_QUIT
};

struct ui_state {
  const char *sdcard_root;
  char plumos_root[PATH_MAX];
  char top_cache_path[PATH_MAX];
  char settings_path[PATH_MAX];
  char system_config_path[PATH_MAX];
  char systems_path[PATH_MAX];
  char wpa_status_path[PATH_MAX];
  char menus_path[PATH_MAX];
  char apps_path[PATH_MAX];
  char favorites_path[PATH_MAX];
  char recent_path[PATH_MAX];
  int show_all;
  int refresh;
  int no_clear;
  int once;
  int renderer_mali;
  int rescue_network;
  int render_failed;
  int fe_ready_flag_written;
  int timeout_sec;
  enum ui_screen screen;
  enum ui_screen rom_entry_screen;
  enum ui_screen back_screen;
  enum ui_screen safe_back_screen;
  enum ui_screen core_back_screen;
  enum ui_screen wifi_back_screen;
  size_t top_cursor;
  size_t top_transition_from_cursor;
  size_t top_transition_from_page;
  size_t top_transition_to_page;
  long long top_transition_start_ms;
  long top_transition_duration_ms;
  int top_transition_direction;
  int top_transition_active;
  size_t gallery_transition_from_cursor;
  size_t gallery_transition_to_cursor;
  long long gallery_transition_start_ms;
  long gallery_transition_duration_ms;
  int gallery_transition_direction;
  int gallery_transition_active;
  size_t gallery_pending_cursor;
  int gallery_pending_active;
  enum ui_screen gallery_back_screen;
  size_t rom_cursor;
  size_t menu_cursor;
  size_t settings_cursor;
  enum settings_category settings_category;
  size_t settings_blink_cursor;
  int settings_blink_direction;
  long long settings_blink_until_ms;
  size_t safe_cursor;
  struct top_entry top_entries[UI_MAX_TOP];
  size_t top_count;
  struct rom_entry rom_entries[UI_MAX_ROMS];
  size_t rom_count;
  struct menu_entry menu_entries[UI_MAX_MENU];
  size_t menu_count;
  char menu_id[64];
  char menu_title[128];
  char thumbnail_result_lines[UI_THUMBNAIL_RESULT_MAX_LINES][UI_RENDER_LINE_MAX];
  size_t thumbnail_result_count;
  size_t thumbnail_result_cursor;
  char thumbnail_running_title[128];
  char thumbnail_running_phase[32];
  char thumbnail_running_system[64];
  long thumbnail_progress_current;
  long thumbnail_progress_total;
  long thumbnail_progress_downloaded;
  long thumbnail_progress_no_match;
  long thumbnail_progress_failed;
  struct scraping_choice scraping_choices[UI_MAX_SCRAPING_CHOICES];
  size_t scraping_choice_count;
  size_t scraping_choice_cursor;
  size_t scraping_menu_cursor;
  size_t scraping_kind_cursor;
  int scraping_replace_existing;
  struct setting_entry setting_entries[UI_MAX_SETTINGS];
  size_t setting_count;
  struct wifi_network_entry wifi_networks[UI_MAX_WIFI_NETWORKS];
  size_t wifi_count;
  size_t wifi_cursor;
  enum wifi_connect_stage wifi_stage;
  long long usb_disk_start_due_ms;
  int manual_time_initialized;
  long manual_time_year;
  long manual_time_month;
  long manual_time_day;
  long manual_time_hour;
  long manual_time_minute;
  size_t wifi_key_row;
  size_t wifi_key_col;
  int wifi_key_shift;
  char wifi_password[UI_WIFI_PASSWORD_MAX + 1];
  char wifi_result_title[96];
  char wifi_result_ip[64];
  char wifi_result_gateway[64];
  char wifi_result_gateway_ping[32];
  char wifi_result_stage[64];
  int wifi_result_success;
  struct frontend_settings frontend_settings;
  struct theme_state theme;
  struct device_settings device;
  char input_event_path[PATH_MAX];
  int input_event_fd;
  long long ignore_input_until_ms;
  enum ui_action repeat_action;
  unsigned int repeat_key_code;
  long long repeat_next_ms;
  long long sdcard_cleanup_last_ms;
  pid_t rom_scan_refresh_pid;
  long long rom_scan_refresh_last_ms;
  char rom_scan_refresh_system_id[64];
  int rom_scan_refresh_suppressed;
  int rom_scan_background_started;
  char mali_rotation[16];
  char mali_tty_entry_scale[8];
  char render_lines[UI_RENDER_MAX_LINES][UI_RENDER_LINE_MAX];
  size_t render_line_count;
#ifdef PLUMOS_ENABLE_MALI_RENDERER
  struct plumos_mali_renderer mali_renderer;
#endif
  char current_system_id[64];
  char current_system_name[128];
  char fb_path[PATH_MAX];
  char egl_path[PATH_MAX];
  char gles_path[PATH_MAX];
  char mali_font_path[PATH_MAX];
  int renderer_active;
  char core_target_system_id[64];
  char core_target_relative_path[UI_PATH_MAX];
  struct core_profile_choice core_profiles[UI_MAX_CORE_PROFILES];
  size_t core_profile_count;
  size_t core_profile_cursor;
  size_t core_menu_cursor;
  char core_current_profile[128];
  char core_current_source[64];
  char core_cpu_policy[32];
  char core_cpu_label[64];
  char core_cpu_source[64];
  char core_cpu_cores_source[64];
  long core_cpu_freq_khz;
  long core_cpu_cores;
  char core_lines[UI_RENDER_MAX_LINES][UI_RENDER_LINE_MAX];
  size_t core_line_count;
  char safe_target_system_id[64];
  char safe_target_relative_path[UI_PATH_MAX];
  char safe_target_launch_profile[128];
  char performance_system_id[64];
  char performance_system_name[128];
  char performance_cpu_policy[32];
  char performance_cpu_label[64];
  long performance_cpu_freq_khz;
  long performance_cpu_cores;
  char status[256];
};

static int load_top_entries(struct ui_state *ui);
static void set_status(struct ui_state *ui, const char *text);

static const struct safe_entry SAFE_ENTRIES[] = {
    {"sleep", "Sleep", "save state, flush SRAM, sync, enter sleep"},
    {"shutdown", "Shutdown", "save state, flush SRAM, sync, power off"},
    {"cancel", "Cancel", "return without changing state"},
};

static const size_t SAFE_ENTRY_COUNT = sizeof(SAFE_ENTRIES) / sizeof(SAFE_ENTRIES[0]);

static const struct performance_cpu_preset PERFORMANCE_CPU_PRESETS[] = {
    {"648 MHz", "fixed", 648000},
    {"816 MHz", "fixed", 816000},
    {"1200 MHz", "fixed", 1200000},
    {"1344 MHz", "fixed", 1344000},
};

static const size_t PERFORMANCE_CPU_PRESET_COUNT =
    sizeof(PERFORMANCE_CPU_PRESETS) / sizeof(PERFORMANCE_CPU_PRESETS[0]);

static const long PERFORMANCE_CPU_CORE_PRESETS[] = {2, 4};
static const size_t PERFORMANCE_CPU_CORE_PRESET_COUNT =
    sizeof(PERFORMANCE_CPU_CORE_PRESETS) / sizeof(PERFORMANCE_CPU_CORE_PRESETS[0]);

enum core_menu_row {
  CORE_MENU_ROW_PROFILE = 0,
  CORE_MENU_ROW_SEPARATOR = 1,
  CORE_MENU_ROW_CPU_FREQ = 2,
  CORE_MENU_ROW_CPU_CORES = 3,
  CORE_MENU_ROW_COUNT = 4,
};

static const char *WIFI_KEYBOARD_ROWS_LOWER[UI_WIFI_COMMAND_ROW] = {
    "0123456789",
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm",
    "-_.@#$%&!*",
    "?+=:/\\\"'()[]"
};

static const char *WIFI_KEYBOARD_ROWS_UPPER[UI_WIFI_COMMAND_ROW] = {
    "0123456789",
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM",
    "-_.@#$%&!*",
    "?+=:/\\\"'()[]"
};

static const char *WIFI_COMMAND_LABELS[UI_WIFI_COMMAND_COUNT] = {
    "SHIFT", "SPACE", "DEL", "CLEAR", "CONNECT"
};

static long long current_time_ms(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    return (long long)time(NULL) * 1000LL;
  }
  return (long long)tv.tv_sec * 1000LL + (long long)(tv.tv_usec / 1000);
}

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

static void copy_truncated_string(char *out, size_t out_size, const char *in) {
  size_t len;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!in) {
    return;
  }
  len = strlen(in);
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, in, len);
  out[len] = '\0';
}

static void copy_prefixed_truncated_string(char *out, size_t out_size,
                                           const char *prefix, const char *value) {
  size_t prefix_len;
  size_t value_len;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!prefix) {
    prefix = "";
  }
  if (!value) {
    value = "";
  }
  prefix_len = strlen(prefix);
  if (prefix_len >= out_size) {
    prefix_len = out_size - 1;
  }
  memcpy(out, prefix, prefix_len);
  out[prefix_len] = '\0';
  if (prefix_len + 1 >= out_size) {
    return;
  }
  value_len = strlen(value);
  if (value_len >= out_size - prefix_len) {
    value_len = out_size - prefix_len - 1;
  }
  memcpy(out + prefix_len, value, value_len);
  out[prefix_len + value_len] = '\0';
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

static int append_size_t(char *out, size_t out_size, size_t *pos, size_t value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%zu", value);
  return append_string(out, out_size, pos, buf);
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

static int dirname_path(char *out, size_t out_size, const char *path) {
  const char *slash;
  size_t len;

  if (!out || out_size == 0 || !path || !path[0]) {
    return 0;
  }
  slash = strrchr(path, '/');
  if (!slash) {
    return copy_string(out, out_size, ".");
  }
  if (slash == path) {
    return copy_string(out, out_size, "/");
  }
  len = (size_t)(slash - path);
  if (len + 1 > out_size) {
    return 0;
  }
  memcpy(out, path, len);
  out[len] = '\0';
  return 1;
}

static int file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
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

static int valid_launch_profile_id(const char *s) {
  if (!s || !s[0]) {
    return 0;
  }
  while (*s) {
    unsigned char c = (unsigned char)*s++;
    if (!(isalnum(c) || c == '_' || c == '-' || c == ':')) {
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

static int read_first_line_file(const char *path, char *out, size_t out_size) {
  FILE *f;
  size_t len;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!path || !path[0]) {
    return 0;
  }
  f = fopen(path, "rb");
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
  while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r' ||
                     isspace((unsigned char)out[len - 1]))) {
    out[--len] = '\0';
  }
  return out[0] != '\0';
}

static void trim_line_end(char *s);

static int read_network_service_status(struct ui_state *ui, const char *service,
                                       char *status, size_t status_size,
                                       int *running_out) {
  char script[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  char line[256];
  char state[64] = "";
  char summary[128] = "";
  char enabled_value[32] = "";
  FILE *pipe;
  size_t pos = 0;
  int running = 0;
  int installed = 1;
  int enabled = 0;
  int enabled_seen = 0;

  if (status && status_size > 0) {
    status[0] = '\0';
  }
  if (running_out) {
    *running_out = 0;
  }
  if (!ui || !service || !service[0] ||
      !join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-network-services")) {
    copy_string(status, status_size, "Not Installed");
    return 0;
  }
  if (!file_exists(script)) {
    copy_string(status, status_size, "Not Installed");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, " status ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, service) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>/dev/null")) {
    copy_string(status, status_size, "Status Error");
    return 0;
  }

  pipe = popen(cmd, "r");
  if (!pipe) {
    copy_string(status, status_size, "Status Error");
    return 0;
  }
  while (fgets(line, sizeof(line), pipe)) {
    trim_line_end(line);
    if (strncmp(line, "state=", 6) == 0) {
      copy_truncated_string(state, sizeof(state), line + 6);
    } else if (strncmp(line, "summary=", 8) == 0) {
      copy_truncated_string(summary, sizeof(summary), line + 8);
    } else if (strncmp(line, "enabled=", 8) == 0) {
      copy_truncated_string(enabled_value, sizeof(enabled_value), line + 8);
      enabled_seen = 1;
    }
  }
  pclose(pipe);

  enabled = strcmp(enabled_value, "1") == 0 ||
            strcmp(enabled_value, "true") == 0 ||
            strcmp(enabled_value, "on") == 0;
  if (strcmp(state, "running") == 0) {
    running = 1;
  } else if (strcmp(state, "not_installed") == 0) {
    installed = 0;
  }
  if (running_out) {
    *running_out = enabled_seen ? enabled : running;
  }
  if (summary[0]) {
    copy_string(status, status_size, summary);
  } else if (state[0]) {
    copy_string(status, status_size, state);
  } else {
    copy_string(status, status_size, installed ? "Stopped" : "Not Installed");
  }
  return installed;
}

static int write_text_file(const char *path, const char *text) {
  int fd;
  size_t len;
  ssize_t written;

  if (!path || !path[0] || !text) {
    return 0;
  }
  fd = open(path, O_WRONLY);
  if (fd < 0) {
    return 0;
  }
  len = strlen(text);
  written = write(fd, text, len);
  if (close(fd) != 0) {
    return 0;
  }
  return written == (ssize_t)len;
}

static int write_text_file_create(const char *path, const char *text) {
  int fd;
  size_t len;
  ssize_t written;

  if (!path || !path[0] || !text) {
    return 0;
  }
  fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return 0;
  }
  len = strlen(text);
  written = write(fd, text, len);
  if (close(fd) != 0) {
    return 0;
  }
  return written == (ssize_t)len;
}

static int write_all_string(int fd, const char *text) {
  size_t len;
  size_t off = 0;

  if (fd < 0 || !text) {
    return 0;
  }
  len = strlen(text);
  while (off < len) {
    ssize_t n = write(fd, text + off, len - off);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return 0;
    }
    if (n == 0) {
      return 0;
    }
    off += (size_t)n;
  }
  return 1;
}

static void trim_line_end(char *s) {
  size_t len;

  if (!s) {
    return;
  }
  len = strlen(s);
  while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
    s[--len] = '\0';
  }
}

static int string_contains_line_break(const char *s) {
  return s && (strchr(s, '\n') || strchr(s, '\r'));
}

static int wifi_network_exists(const struct ui_state *ui, const char *ssid) {
  size_t i;

  if (!ui || !ssid || !ssid[0]) {
    return 0;
  }
  for (i = 0; i < ui->wifi_count; i++) {
    if (strcmp(ui->wifi_networks[i].ssid, ssid) == 0) {
      return 1;
    }
  }
  return 0;
}

static void wifi_add_network(struct ui_state *ui, const char *ssid,
                             const char *security, const char *signal) {
  struct wifi_network_entry *entry;

  if (!ui || !ssid || !ssid[0] || ui->wifi_count >= UI_MAX_WIFI_NETWORKS ||
      wifi_network_exists(ui, ssid)) {
    return;
  }
  entry = &ui->wifi_networks[ui->wifi_count++];
  copy_truncated_string(entry->ssid, sizeof(entry->ssid), ssid);
  copy_truncated_string(entry->security, sizeof(entry->security),
                        security && security[0] ? security : "secured");
  copy_truncated_string(entry->signal, sizeof(entry->signal),
                        signal && signal[0] ? signal : "-");
}

static int wifi_scan_networks(struct ui_state *ui) {
  char script[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  char line[512];
  FILE *pipe;
  size_t pos = 0;
  int rc;

  if (!ui) {
    return 0;
  }
  ui->wifi_count = 0;
  ui->wifi_cursor = 0;
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-network-control")) {
    set_status(ui, "network control path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "network control script missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, " --scan 2>/dev/null")) {
    set_status(ui, "Wi-Fi scan command too long");
    return 0;
  }

  pipe = popen(cmd, "r");
  if (!pipe) {
    set_status(ui, "Wi-Fi scan failed to start");
    return 0;
  }
  while (fgets(line, sizeof(line), pipe)) {
    char *security;
    char *signal;
    char *ssid;
    char *tab;

    trim_line_end(line);
    if (strncmp(line, "network\t", 8) != 0) {
      continue;
    }
    security = line + 8;
    tab = strchr(security, '\t');
    if (!tab) {
      continue;
    }
    *tab = '\0';
    signal = tab + 1;
    tab = strchr(signal, '\t');
    if (!tab) {
      continue;
    }
    *tab = '\0';
    ssid = tab + 1;
    if (!ssid[0]) {
      continue;
    }
    wifi_add_network(ui, ssid, security, signal);
  }
  rc = pclose(pipe);
  if (ui->wifi_count == 0) {
    if (rc == -1 || !(WIFEXITED(rc) && WEXITSTATUS(rc) == 0)) {
      set_status(ui, "Wi-Fi scan failed");
      return 0;
    }
    set_status(ui, "No Wi-Fi networks found");
    return 1;
  }
  snprintf(ui->status, sizeof(ui->status), "Wi-Fi scan found %zu", ui->wifi_count);
  return 1;
}

static const char *wifi_keyboard_row_chars(const struct ui_state *ui, size_t row) {
  if (row >= UI_WIFI_COMMAND_ROW) {
    return "";
  }
  return ui && ui->wifi_key_shift ? WIFI_KEYBOARD_ROWS_UPPER[row]
                                  : WIFI_KEYBOARD_ROWS_LOWER[row];
}

static size_t wifi_keyboard_row_len(const struct ui_state *ui, size_t row) {
  if (row == UI_WIFI_COMMAND_ROW) {
    return UI_WIFI_COMMAND_COUNT;
  }
  if (row >= UI_WIFI_KEYBOARD_ROWS) {
    return 0;
  }
  return strlen(wifi_keyboard_row_chars(ui, row));
}

static void wifi_clamp_key_cursor(struct ui_state *ui) {
  size_t row_len;

  if (!ui) {
    return;
  }
  if (ui->wifi_key_row >= UI_WIFI_KEYBOARD_ROWS) {
    ui->wifi_key_row = UI_WIFI_KEYBOARD_ROWS - 1;
  }
  row_len = wifi_keyboard_row_len(ui, ui->wifi_key_row);
  if (row_len == 0) {
    ui->wifi_key_col = 0;
  } else if (ui->wifi_key_col >= row_len) {
    ui->wifi_key_col = row_len - 1;
  }
}

static void wifi_append_password_char(struct ui_state *ui, char c) {
  size_t len;

  if (!ui) {
    return;
  }
  len = strlen(ui->wifi_password);
  if (len >= UI_WIFI_PASSWORD_MAX) {
    set_status(ui, "Password is already 64 chars");
    return;
  }
  ui->wifi_password[len] = c;
  ui->wifi_password[len + 1] = '\0';
  snprintf(ui->status, sizeof(ui->status), "Password length %zu", len + 1);
}

static void wifi_delete_password_char(struct ui_state *ui) {
  size_t len;

  if (!ui) {
    return;
  }
  len = strlen(ui->wifi_password);
  if (len > 0) {
    ui->wifi_password[len - 1] = '\0';
  }
  snprintf(ui->status, sizeof(ui->status), "Password length %zu",
           strlen(ui->wifi_password));
}

static void wifi_format_keyboard_row(const struct ui_state *ui, size_t row,
                                     char *out, size_t out_size) {
  size_t pos = 0;
  size_t i;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (row == UI_WIFI_COMMAND_ROW) {
    for (i = 0; i < UI_WIFI_COMMAND_COUNT; i++) {
      if (i > 0 && !append_string(out, out_size, &pos, " ")) {
        return;
      }
      if (!append_string(out, out_size, &pos, WIFI_COMMAND_LABELS[i])) {
        return;
      }
    }
    return;
  }
  if (row < UI_WIFI_KEYBOARD_ROWS) {
    const char *keys = wifi_keyboard_row_chars(ui, row);
    for (i = 0; keys[i]; i++) {
      char ch[2];
      if (i > 0 && !append_string(out, out_size, &pos, " ")) {
        return;
      }
      ch[0] = keys[i];
      ch[1] = '\0';
      if (!append_string(out, out_size, &pos, ch)) {
        return;
      }
    }
  }
}

static long scale_setting_to_runtime(long value, long setting_max,
                                     long runtime_max) {
  value = clamp_long(value, 0, setting_max);
  if (setting_max <= 0) {
    return 0;
  }
  return (value * runtime_max + setting_max / 2) / setting_max;
}

static int system_command_succeeded(int rc) {
  return rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

static int command_succeeds_quiet(const char *cmd) {
  if (!cmd || !cmd[0]) {
    return 0;
  }
  return system_command_succeeded(system(cmd));
}

static const char *plumos_default_timezone(void) {
  return "JST-9";
}

static int valid_timezone_value(const char *timezone) {
  const unsigned char *p = (const unsigned char *)timezone;

  if (!timezone || !timezone[0]) {
    return 0;
  }
  while (*p) {
    if (!(isalnum(*p) || *p == '_' || *p == '-' || *p == '+' ||
          *p == '/' || *p == ':' || *p == ',' || *p == '.')) {
      return 0;
    }
    p++;
  }
  return 1;
}

static void apply_plumos_timezone_value(const char *timezone) {
  const char *value = valid_timezone_value(timezone) ? timezone : plumos_default_timezone();

  setenv("TZ", value, 1);
  tzset();
}

static void format_current_time_local(char *out, size_t out_size) {
  time_t now;
  struct tm tm_value;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  now = time(NULL);
  if (now == (time_t)-1 || !localtime_r(&now, &tm_value)) {
    copy_string(out, out_size, "unavailable");
    return;
  }
  if (strftime(out, out_size, "%Y-%m-%d %H:%M", &tm_value) == 0) {
    copy_string(out, out_size, "unavailable");
  }
}

static int runtime_wifi_enabled(void) {
  char operstate[32];

  if (read_first_line_file("/sys/class/net/wlan0/operstate",
                           operstate, sizeof(operstate)) &&
      strcmp(operstate, "down") != 0) {
    return 1;
  }
  if (command_succeeds_quiet(
          "ifconfig wlan0 2>/dev/null | grep -q 'UP' >/dev/null 2>&1")) {
    return 1;
  }
  if (command_succeeds_quiet(
          "ps w 2>/dev/null | grep '[w]pa_supplicant' >/dev/null 2>&1")) {
    return 1;
  }
  return 0;
}

static const char *runtime_volume_backend_path(void) {
  if (access("/usr/bin/amixer", X_OK) == 0) {
    return "/usr/bin/amixer";
  }
  if (access("/bin/amixer", X_OK) == 0) {
    return "/bin/amixer";
  }
  if (access("/sbin/amixer", X_OK) == 0) {
    return "/sbin/amixer";
  }
  return NULL;
}

static const char *runtime_plumos_root(void) {
  const char *root = getenv("PLUMOS_ROOT");
  return root && root[0] ? root : "/mnt/SDCARD/plumos";
}

static int runtime_volume_control_path(char *out, size_t out_size) {
  return join_path(out, out_size, runtime_plumos_root(), "bin/plumos-volume-control");
}

static int run_volume_control_command(const char *action, long volume,
                                      int include_volume) {
  char helper[PATH_MAX];
  char volume_buf[32];
  char cmd[PATH_MAX + 256];
  size_t pos = 0;

  if (!runtime_volume_control_path(helper, sizeof(helper)) ||
      access(helper, X_OK) != 0) {
    return 0;
  }
  if (include_volume) {
    snprintf(volume_buf, sizeof(volume_buf), "%ld", clamp_long(volume, 0, 20));
  } else {
    volume_buf[0] = '\0';
  }
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, runtime_plumos_root()) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, helper) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, action)) {
    return 0;
  }
  if (include_volume &&
      (!append_string(cmd, sizeof(cmd), &pos, " ") ||
       !append_shell_quoted(cmd, sizeof(cmd), &pos, volume_buf))) {
    return 0;
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " >/tmp/.plumos_volume_set 2>&1")) {
    return 0;
  }
  return system_command_succeeded(system(cmd));
}

static int runtime_volume_backend_available(void) {
  const char *amixer_path = runtime_volume_backend_path();
  char cmd[256];

  if (run_volume_control_command("init", 0, 0)) {
    return 1;
  }
  if (!amixer_path) {
    return 0;
  }
  snprintf(cmd, sizeof(cmd),
           "%s cget iface=MIXER,name='Soft Volume Master' >/dev/null 2>&1",
           amixer_path);
  return system_command_succeeded(system(cmd));
}

static int runtime_lcd_backend_available(void) {
  return access(A30_LCD_BACKLIGHT_PATH, W_OK) == 0;
}

static int runtime_enhance_backend_available(void) {
  return access(A30_DISPLAY_ENHANCE_PATH, W_OK) == 0;
}

static void update_device_backend_status(struct device_settings *device) {
  int lcd_available;
  int enhance_available;

  if (!device) {
    return;
  }
  if (runtime_volume_backend_available()) {
    copy_string(device->volume_backend, sizeof(device->volume_backend),
                "ALSA Soft Volume Master");
  } else if (runtime_volume_backend_path()) {
    copy_string(device->volume_backend, sizeof(device->volume_backend),
                "ALSA softvol unavailable");
  } else {
    copy_string(device->volume_backend, sizeof(device->volume_backend),
                "amixer unavailable");
  }

  lcd_available = runtime_lcd_backend_available();
  enhance_available = runtime_enhance_backend_available();
  if (lcd_available && enhance_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "disp attr lcdbl/enhance");
  } else if (lcd_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "disp attr lcdbl only");
  } else if (enhance_available) {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "disp attr enhance only");
  } else {
    copy_string(device->brightness_backend, sizeof(device->brightness_backend),
                "disp attr unavailable");
  }
}

static int apply_runtime_volume(const struct device_settings *device) {
  char cmd[256];
  const char *amixer_path;
  long mixer_value;

  if (!device) {
    return 0;
  }
  if (run_volume_control_command("apply", device->volume, 1)) {
    return 1;
  }
  amixer_path = runtime_volume_backend_path();
  if (!amixer_path) {
    return 0;
  }
  mixer_value = scale_setting_to_runtime(device->volume, 20, 255);
  snprintf(cmd, sizeof(cmd),
           "%s cset iface=MIXER,name='Soft Volume Master' %ld "
           ">/tmp/.plumos_volume_set 2>&1",
           amixer_path, mixer_value);
  return system_command_succeeded(system(cmd));
}

static long brightness_raw_value(long brightness) {
  static const long raw_values[] = {
      2, 3, 4, 5, 6, 7, 8, 9, 10, 26,
      43, 59, 75, 92, 108, 125, 141, 157, 174, 190,
  };

  brightness = clamp_long(brightness, 1, 20);
  return raw_values[brightness - 1];
}

static long brightness_setting_from_raw(long raw) {
  long best_setting = 1;
  long best_delta = labs(raw - brightness_raw_value(1));
  long setting;

  for (setting = 2; setting <= 20; setting++) {
    long candidate = brightness_raw_value(setting);
    long delta = labs(raw - candidate);
    if (delta < best_delta) {
      best_setting = setting;
      best_delta = delta;
    }
  }
  return best_setting;
}

static long brightness_setting_from_stored(long stored) {
  if (stored >= 1 && stored <= 20) {
    return stored;
  }
  if (stored < 1) {
    return 1;
  }
  return brightness_setting_from_raw(stored);
}

static int apply_runtime_brightness(const struct device_settings *device) {
  char value[64];
  long backlight_value;
  long brightness;

  if (!device || !runtime_lcd_backend_available()) {
    return 0;
  }
  brightness = clamp_long(device->brightness, 1, 20);
  backlight_value = brightness_raw_value(brightness);
  snprintf(value, sizeof(value), "%ld\n", backlight_value);
  return write_text_file(A30_LCD_BACKLIGHT_PATH, value);
}

static int apply_runtime_display_enhance(const struct device_settings *device) {
  char value[128];
  long lumination_value;
  long contrast_value;
  long hue_value;
  long saturation_value;

  if (!device || !runtime_enhance_backend_available()) {
    return 0;
  }
  lumination_value = scale_setting_to_runtime(device->lumination, 10, 50);
  contrast_value = scale_setting_to_runtime(device->contrast, 20, 100);
  hue_value = scale_setting_to_runtime(device->hue, 20, 100);
  saturation_value = scale_setting_to_runtime(device->saturation, 20, 100);
  snprintf(value, sizeof(value), "1,%ld,%ld,%ld,%ld\n", lumination_value,
           contrast_value, hue_value, saturation_value);
  return write_text_file(A30_DISPLAY_ENHANCE_PATH, value);
}

static void set_device_setting_number(struct device_settings *device,
                                      const char *id, long value) {
  if (!device || !id) {
    return;
  }
  if (strcmp(id, "system_volume") == 0) {
    device->volume = value;
  } else if (strcmp(id, "system_brightness") == 0) {
    device->brightness = value;
  } else if (strcmp(id, "system_lumination") == 0) {
    device->lumination = value;
  } else if (strcmp(id, "system_contrast") == 0) {
    device->contrast = value;
  } else if (strcmp(id, "system_hue") == 0) {
    device->hue = value;
  } else if (strcmp(id, "system_saturation") == 0) {
    device->saturation = value;
  }
}

static int apply_device_runtime_settings(const struct device_settings *device,
                                         const char *id, char *status,
                                         size_t status_size) {
  int ok = 1;
  int attempted = 0;
  int needs_volume;
  int needs_brightness;
  int needs_enhance;

  if (status && status_size > 0) {
    status[0] = '\0';
  }
  if (!device) {
    if (status && status_size > 0) {
      copy_string(status, status_size, "runtime settings unavailable");
    }
    return 0;
  }

  needs_volume = !id || strcmp(id, "system_volume") == 0;
  needs_brightness = !id || strcmp(id, "system_brightness") == 0;
  needs_enhance = !id || strcmp(id, "system_lumination") == 0 ||
                  strcmp(id, "system_contrast") == 0 ||
                  strcmp(id, "system_hue") == 0 ||
                  strcmp(id, "system_saturation") == 0;

  if (needs_volume) {
    attempted = 1;
    if (!apply_runtime_volume(device)) {
      ok = 0;
    }
  }
  if (needs_brightness && runtime_lcd_backend_available()) {
    attempted = 1;
    if (!apply_runtime_brightness(device)) {
      ok = 0;
    }
  } else if (needs_brightness) {
    ok = 0;
  }
  if (needs_enhance && runtime_enhance_backend_available()) {
    attempted = 1;
    if (!apply_runtime_display_enhance(device)) {
      ok = 0;
    }
  } else if (needs_enhance) {
    ok = 0;
  }

  if (!ok && status && status_size > 0) {
    copy_string(status, status_size,
                attempted ? "runtime apply failed" : "runtime backend unavailable");
  }
  return ok;
}

static void apply_frontend_cpu_default(void) {
  const char *disabled = getenv("PLUMOS_CONTROLLER_CPU_DEFAULT");
  const char *freq = "648000\n";

  if (disabled && (strcmp(disabled, "0") == 0 || strcmp(disabled, "off") == 0 ||
                   strcmp(disabled, "false") == 0)) {
    return;
  }

  write_text_file("/sys/devices/system/cpu/cpu1/online", "1\n");
  write_text_file("/sys/devices/system/cpu/cpu2/online", "0\n");
  write_text_file("/sys/devices/system/cpu/cpu3/online", "0\n");

  write_text_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", freq);
  write_text_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", freq);
  write_text_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "userspace\n");
  write_text_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed", freq);
}

static void format_storage_status(const char *path, char *out, size_t out_size) {
  struct statvfs st;
  unsigned long long total_mb;
  unsigned long long free_mb;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!path || !path[0] || statvfs(path, &st) != 0 || st.f_frsize == 0) {
    copy_string(out, out_size, "unavailable");
    return;
  }
  total_mb = ((unsigned long long)st.f_blocks * (unsigned long long)st.f_frsize) /
             (1024ULL * 1024ULL);
  free_mb = ((unsigned long long)st.f_bavail * (unsigned long long)st.f_frsize) /
            (1024ULL * 1024ULL);
  snprintf(out, out_size, "%lluMB free / %lluMB total", free_mb, total_mb);
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

static int parse_nonnegative_long(const char *value, long *out) {
  char *endptr = NULL;
  long parsed;

  if (!value || !value[0] || !out) {
    return 0;
  }
  errno = 0;
  parsed = strtol(value, &endptr, 10);
  if (errno != 0 || endptr == value || *endptr != '\0' || parsed < 0) {
    return 0;
  }
  *out = parsed;
  return 1;
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

static int run_scanner(const char *plumos_root, const char *sdcard_root, const char *system_id,
                       int with_thumbnails) {
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
    if (with_thumbnails && !append_string(cmd, sizeof(cmd), &pos, " --with-thumbnails")) {
      return 0;
    }
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " >/dev/null")) {
    return 0;
  }

  rc = system(cmd);
  return rc == 0;
}

static void redirect_child_stdio_to_devnull(void) {
  int fd = open("/dev/null", O_RDWR);
  if (fd < 0) {
    return;
  }
  dup2(fd, STDIN_FILENO);
  dup2(fd, STDOUT_FILENO);
  dup2(fd, STDERR_FILENO);
  if (fd > STDERR_FILENO) {
    close(fd);
  }
}

static void close_child_extra_fds(void) {
  long max_fd = sysconf(_SC_OPEN_MAX);
  int fd;

  if (max_fd < 0 || max_fd > 256) {
    max_fd = 256;
  }
  for (fd = STDERR_FILENO + 1; fd < max_fd; fd++) {
    close(fd);
  }
}

static pid_t start_scanner_process(const char *plumos_root, const char *sdcard_root,
                                   const char *system_id, int with_thumbnails) {
  char scanner[PATH_MAX];
  pid_t pid;

  if (!join_path(scanner, sizeof(scanner), plumos_root, "bin/plumos-library-scan")) {
    return -1;
  }
  if (!file_exists(scanner)) {
    return -1;
  }
  if (system_id && !valid_system_id(system_id)) {
    return -1;
  }

  pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    setenv("PLUMOS_ROOT", plumos_root, 1);
    setenv("PLUMOS_SDCARD_ROOT", sdcard_root, 1);
    redirect_child_stdio_to_devnull();
    close_child_extra_fds();
    if (system_id) {
      if (with_thumbnails) {
        execl(scanner, scanner, "--on-enter", system_id, "--with-thumbnails",
              (char *)NULL);
      } else {
        execl(scanner, scanner, "--on-enter", system_id, (char *)NULL);
      }
    } else {
      execl(scanner, scanner, (char *)NULL);
    }
    _exit(127);
  }
  return pid;
}

static int trigger_rom_scan_refresh(struct ui_state *ui, const char *system_id,
                                    int with_thumbnails) {
  long long now;
  pid_t pid;

  if (!ui || !system_id || !valid_system_id(system_id)) {
    return 0;
  }
  if (ui->rom_scan_refresh_pid > 0) {
    return 0;
  }
  now = current_time_ms();
  if (ui->rom_scan_refresh_last_ms > 0 &&
      strcmp(ui->rom_scan_refresh_system_id, system_id) == 0 &&
      now - ui->rom_scan_refresh_last_ms < UI_ROM_SCAN_REFRESH_MIN_INTERVAL_MS) {
    return 0;
  }

  pid = start_scanner_process(ui->plumos_root, ui->sdcard_root, system_id,
                              with_thumbnails);
  if (pid <= 0) {
    return 0;
  }
  ui->rom_scan_refresh_pid = pid;
  ui->rom_scan_refresh_last_ms = now;
  copy_string(ui->rom_scan_refresh_system_id, sizeof(ui->rom_scan_refresh_system_id),
              system_id);
  ui->rom_scan_background_started = 1;
  return 1;
}

static int start_sdcard_cleanup_process(const char *plumos_root, const char *sdcard_root,
                                        int force, int invalidate_cache) {
  char cleanup[PATH_MAX];
  pid_t pid;

  if (!join_path(cleanup, sizeof(cleanup), plumos_root, "bin/plumos-sdcard-cleanup")) {
    return 0;
  }
  if (!file_exists(cleanup)) {
    return 0;
  }

  pid = fork();
  if (pid < 0) {
    return 0;
  }
  if (pid == 0) {
    setenv("PLUMOS_ROOT", plumos_root, 1);
    setenv("PLUMOS_SDCARD_ROOT", sdcard_root, 1);
    redirect_child_stdio_to_devnull();
    close_child_extra_fds();
    if (force && invalidate_cache) {
      execl(cleanup, cleanup, "--force", (char *)NULL);
    } else if (force && !invalidate_cache) {
      execl(cleanup, cleanup, "--force", "--no-cache-invalidate", (char *)NULL);
    } else if (!force && !invalidate_cache) {
      execl(cleanup, cleanup, "--no-cache-invalidate", (char *)NULL);
    } else {
      execl(cleanup, cleanup, (char *)NULL);
    }
    _exit(127);
  }
  return 1;
}

static void trigger_sdcard_cleanup_from_start_menu(struct ui_state *ui) {
  long long now;

  if (!ui) {
    return;
  }
  now = current_time_ms();
  if (ui->sdcard_cleanup_last_ms > 0 &&
      now - ui->sdcard_cleanup_last_ms < UI_SDCARD_CLEANUP_MIN_INTERVAL_MS) {
    return;
  }
  ui->sdcard_cleanup_last_ms = now;
  start_sdcard_cleanup_process(ui->plumos_root, ui->sdcard_root, 1, 1);
}

static void init_frontend_settings(struct frontend_settings *settings) {
  memset(settings, 0, sizeof(*settings));
  settings->show_favorites_on_top = 1;
  settings->show_recent_on_top = 1;
  settings->rom_scan_slow_threshold_ms = 500;
  settings->rom_scan_test_file_count = 1000;
  copy_string(settings->boot_resume_mode, sizeof(settings->boot_resume_mode), "off");
  copy_string(settings->ui_mode, sizeof(settings->ui_mode), "text");
  copy_string(settings->top_mode, sizeof(settings->top_mode), "text");
  copy_string(settings->rom_mode, sizeof(settings->rom_mode), "text");
  copy_string(settings->theme_id, sizeof(settings->theme_id), "default");
  copy_string(settings->graphic_theme_id, sizeof(settings->graphic_theme_id), "default");
  settings->graphic_transition_ms = 0;
  copy_string(settings->sort_systems, sizeof(settings->sort_systems), "sort_order");
  copy_string(settings->sort_roms, sizeof(settings->sort_roms), "name");
  copy_string(settings->rom_scan_policy, sizeof(settings->rom_scan_policy), "on_enter");
}

static int load_settings(const char *path, struct frontend_settings *settings) {
  char *json;
  size_t json_size;

  init_frontend_settings(settings);
  if (!file_exists(path)) {
    return 1;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  settings->show_empty_systems = json_get_bool(json, json + json_size, "show_empty_systems", 0);
  settings->show_favorites_on_top =
      json_get_bool(json, json + json_size, "show_favorites_on_top", 1);
  settings->show_recent_on_top =
      json_get_bool(json, json + json_size, "show_recent_on_top", 1);
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
  if (!json_get_string(json, json + json_size, "graphic_theme_id",
                       settings->graphic_theme_id,
                       sizeof(settings->graphic_theme_id)) &&
      settings->theme_id[0]) {
    copy_string(settings->graphic_theme_id, sizeof(settings->graphic_theme_id),
                settings->theme_id);
  }
  json_get_string(json, json + json_size, "graphic_top_layout",
                  settings->graphic_top_layout,
                  sizeof(settings->graphic_top_layout));
  json_get_string(json, json + json_size, "graphic_transition",
                  settings->graphic_transition,
                  sizeof(settings->graphic_transition));
  json_get_string(json, json + json_size, "graphic_transition_axis",
                  settings->graphic_transition_axis,
                  sizeof(settings->graphic_transition_axis));
  json_get_string(json, json + json_size, "graphic_transition_easing",
                  settings->graphic_transition_easing,
                  sizeof(settings->graphic_transition_easing));
  settings->graphic_transition_ms =
      json_get_long(json, json + json_size, "graphic_transition_ms", 0);
  json_get_string(json, json + json_size, "sort_systems", settings->sort_systems,
                  sizeof(settings->sort_systems));
  json_get_string(json, json + json_size, "sort_roms", settings->sort_roms,
                  sizeof(settings->sort_roms));
  json_get_string(json, json + json_size, "rom_scan_policy", settings->rom_scan_policy,
                  sizeof(settings->rom_scan_policy));
  settings->rom_scan_slow_threshold_ms =
      json_get_long(json, json + json_size, "rom_scan_slow_threshold_ms", 500);
  settings->rom_scan_test_file_count =
      json_get_long(json, json + json_size, "rom_scan_test_file_count", 1000);
  json_get_string(json, json + json_size, "last_system_id", settings->last_system_id,
                  sizeof(settings->last_system_id));
  free(json);
  return 1;
}

static void fprint_json_string(FILE *f, const char *s) {
  const unsigned char *p = (const unsigned char *)(s ? s : "");

  fputc('"', f);
  while (*p) {
    if (*p == '"' || *p == '\\') {
      fputc('\\', f);
      fputc(*p, f);
    } else if (*p == '\n') {
      fputs("\\n", f);
    } else if (*p == '\r') {
      fputs("\\r", f);
    } else if (*p == '\t') {
      fputs("\\t", f);
    } else if (*p < 0x20) {
      fprintf(f, "\\u%04x", (unsigned int)*p);
    } else {
      fputc(*p, f);
    }
    p++;
  }
  fputc('"', f);
}

static int append_json_string_literal(char *out, size_t out_size, const char *s) {
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  size_t pos = 0;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (!append_string(out, out_size, &pos, "\"")) {
    return 0;
  }
  while (*p) {
    char escaped[8];
    if (*p == '"' || *p == '\\') {
      escaped[0] = '\\';
      escaped[1] = (char)*p;
      escaped[2] = '\0';
      if (!append_string(out, out_size, &pos, escaped)) {
        return 0;
      }
    } else if (*p == '\n') {
      if (!append_string(out, out_size, &pos, "\\n")) {
        return 0;
      }
    } else if (*p == '\r') {
      if (!append_string(out, out_size, &pos, "\\r")) {
        return 0;
      }
    } else if (*p == '\t') {
      if (!append_string(out, out_size, &pos, "\\t")) {
        return 0;
      }
    } else if (*p < 0x20) {
      snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned int)*p);
      if (!append_string(out, out_size, &pos, escaped)) {
        return 0;
      }
    } else {
      if (pos + 2 > out_size) {
        return 0;
      }
      out[pos++] = (char)*p;
      out[pos] = '\0';
    }
    p++;
  }
  return append_string(out, out_size, &pos, "\"");
}

static int save_settings(const char *path, const struct frontend_settings *settings) {
  char tmp_path[PATH_MAX];
  FILE *f;
  int fd;

  if (!path || !settings ||
      snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {
    return 0;
  }
  f = fopen(tmp_path, "wb");
  if (!f) {
    return 0;
  }
  fprintf(f, "{\n  \"version\": 1,\n");
  fprintf(f, "  \"ui_mode\": ");
  fprint_json_string(f, settings->ui_mode);
  fprintf(f, ",\n  \"top_mode\": ");
  fprint_json_string(f, settings->top_mode);
  fprintf(f, ",\n  \"rom_mode\": ");
  fprint_json_string(f, settings->rom_mode);
  fprintf(f, ",\n  \"show_empty_systems\": %s,\n",
          settings->show_empty_systems ? "true" : "false");
  fprintf(f, "  \"show_favorites_on_top\": %s,\n",
          settings->show_favorites_on_top ? "true" : "false");
  fprintf(f, "  \"show_recent_on_top\": %s,\n",
          settings->show_recent_on_top ? "true" : "false");
  fprintf(f, "  \"boot_resume_mode\": ");
  fprint_json_string(f, settings->boot_resume_mode);
  fprintf(f, ",\n  \"sort_systems\": ");
  fprint_json_string(f, settings->sort_systems);
  fprintf(f, ",\n  \"sort_roms\": ");
  fprint_json_string(f, settings->sort_roms);
  fprintf(f, ",\n  \"rom_scan_policy\": ");
  fprint_json_string(f, settings->rom_scan_policy);
  fprintf(f, ",\n  \"rom_scan_slow_threshold_ms\": %ld,\n",
          settings->rom_scan_slow_threshold_ms);
  fprintf(f, "  \"rom_scan_test_file_count\": %ld,\n",
          settings->rom_scan_test_file_count);
  fprintf(f, "  \"theme_id\": ");
  fprint_json_string(f, settings->graphic_theme_id);
  fprintf(f, ",\n  \"graphic_theme_id\": ");
  fprint_json_string(f, settings->graphic_theme_id);
  fprintf(f, ",\n  \"graphic_top_layout\": ");
  fprint_json_string(f, settings->graphic_top_layout);
  fprintf(f, ",\n  \"graphic_transition\": ");
  fprint_json_string(f, settings->graphic_transition);
  fprintf(f, ",\n  \"graphic_transition_ms\": %ld,\n",
          settings->graphic_transition_ms);
  fprintf(f, "  \"graphic_transition_axis\": ");
  fprint_json_string(f, settings->graphic_transition_axis);
  fprintf(f, ",\n  \"graphic_transition_easing\": ");
  fprint_json_string(f, settings->graphic_transition_easing);
  fprintf(f, ",\n  \"last_system_id\": ");
  fprint_json_string(f, settings->last_system_id);
  fprintf(f, "\n}\n");

  fd = fileno(f);
  if (fflush(f) != 0 || (fd >= 0 && fsync(fd) != 0) || fclose(f) != 0) {
    unlink(tmp_path);
    return 0;
  }
  if (rename(tmp_path, path) != 0) {
    unlink(tmp_path);
    return 0;
  }
  sync();
  return 1;
}

static const char *json_value_end(const char *value, const char *end) {
  const char *p;
  const char *body_start;
  const char *body_end;
  const char *after;
  int escape = 0;

  if (!value || value >= end) {
    return NULL;
  }
  p = skip_ws_range(value, end);
  if (p >= end) {
    return NULL;
  }
  if (*p == '"') {
    p++;
    while (p < end) {
      if (escape) {
        escape = 0;
        p++;
        continue;
      }
      if (*p == '\\') {
        escape = 1;
        p++;
        continue;
      }
      if (*p == '"') {
        return p + 1;
      }
      p++;
    }
    return NULL;
  }
  if (*p == '{') {
    if (json_match_container(p, end, '{', '}', &body_start, &body_end, &after)) {
      (void)body_start;
      (void)body_end;
      return after;
    }
    return NULL;
  }
  if (*p == '[') {
    if (json_match_container(p, end, '[', ']', &body_start, &body_end, &after)) {
      (void)body_start;
      (void)body_end;
      return after;
    }
    return NULL;
  }
  while (p < end && *p != ',' && *p != '}' && *p != ']') {
    p++;
  }
  while (p > value && isspace((unsigned char)p[-1])) {
    p--;
  }
  return p > value ? p : NULL;
}

static int write_buffer_atomic(const char *path, const char *tmp_path,
                               const char *data, size_t data_size) {
  FILE *f;
  int fd;

  f = fopen(tmp_path, "wb");
  if (!f) {
    return 0;
  }
  if (data_size > 0 && fwrite(data, 1, data_size, f) != data_size) {
    fclose(f);
    unlink(tmp_path);
    return 0;
  }
  fd = fileno(f);
  if (fflush(f) != 0 || (fd >= 0 && fsync(fd) != 0) || fclose(f) != 0) {
    unlink(tmp_path);
    return 0;
  }
  if (rename(tmp_path, path) != 0) {
    unlink(tmp_path);
    return 0;
  }
  sync();
  return 1;
}

static int ensure_system_config_backup(const char *path, const char *data, size_t data_size) {
  char backup_path[PATH_MAX];
  char backup_tmp_path[PATH_MAX];

  if (!path || !data ||
      snprintf(backup_path, sizeof(backup_path), "%s.plumos.bak", path) >=
          (int)sizeof(backup_path) ||
      snprintf(backup_tmp_path, sizeof(backup_tmp_path), "%s.tmp", backup_path) >=
          (int)sizeof(backup_tmp_path)) {
    return 0;
  }
  if (file_exists(backup_path)) {
    return 1;
  }
  return write_buffer_atomic(backup_path, backup_tmp_path, data, data_size);
}

static int replace_json_key_value_atomic(const char *path, const char *key,
                                         const char *literal) {
  char *json;
  char tmp_path[PATH_MAX];
  size_t json_size;
  const char *json_end;
  const char *value = NULL;
  const char *end_value = NULL;
  const char *insert_at = NULL;
  const char *content_end = NULL;
  const char *object_start = NULL;
  const char *body = NULL;
  int append_key = 0;
  int has_entries = 0;
  FILE *f;
  int fd;
  int write_ok = 0;
  int ok = 0;

  if (!path || !key || !key[0] || !literal ||
      snprintf(tmp_path, sizeof(tmp_path), "%s.plumos.tmp", path) >=
          (int)sizeof(tmp_path)) {
    return 0;
  }
  json = read_file(path, &json_size);
  if (!json) {
    return 0;
  }
  json_end = json + json_size;
  if (!json_find_key_value(json, json_end, key, &value)) {
    object_start = skip_ws_range(json, json_end);
    if (object_start >= json_end || *object_start != '{') {
      free(json);
      return 0;
    }
    insert_at = json_end;
    while (insert_at > object_start && isspace((unsigned char)insert_at[-1])) {
      insert_at--;
    }
    if (insert_at <= object_start || insert_at[-1] != '}') {
      free(json);
      return 0;
    }
    insert_at--;
    content_end = insert_at;
    while (content_end > object_start + 1 &&
           isspace((unsigned char)content_end[-1])) {
      content_end--;
    }
    body = skip_ws_range(object_start + 1, content_end);
    has_entries = body < content_end;
    append_key = 1;
  } else {
    value = skip_ws_range(value, json_end);
    end_value = json_value_end(value, json_end);
    if (!end_value || end_value < value) {
      free(json);
      return 0;
    }
  }
  if (!ensure_system_config_backup(path, json, json_size)) {
    free(json);
    return 0;
  }

  f = fopen(tmp_path, "wb");
  if (!f) {
    free(json);
    return 0;
  }
  if (append_key) {
    write_ok = fwrite(json, 1, (size_t)(content_end - json), f) ==
               (size_t)(content_end - json);
    if (write_ok && has_entries) {
      write_ok = fputs(",\n", f) >= 0;
    }
    if (write_ok && !has_entries) {
      write_ok = fputs("\n", f) >= 0;
    }
    if (write_ok) {
      write_ok = fputs("  ", f) >= 0;
    }
    if (write_ok) {
      fprint_json_string(f, key);
      write_ok = ferror(f) == 0 && fprintf(f, ": %s\n", literal) >= 0;
    }
    if (write_ok) {
      write_ok = fputc('}', f) != EOF &&
                 fwrite(insert_at + 1, 1, (size_t)(json_end - (insert_at + 1)), f) ==
                     (size_t)(json_end - (insert_at + 1));
    }
  } else {
    write_ok = fwrite(json, 1, (size_t)(value - json), f) ==
                   (size_t)(value - json) &&
               fputs(literal, f) >= 0 &&
               fwrite(end_value, 1, (size_t)(json_end - end_value), f) ==
                   (size_t)(json_end - end_value);
  }
  if (write_ok) {
    fd = fileno(f);
    if (fflush(f) == 0 && (fd < 0 || fsync(fd) == 0)) {
      int close_ok = fclose(f) == 0;
      f = NULL;
      if (close_ok && rename(tmp_path, path) == 0) {
        sync();
        ok = 1;
      }
    }
  }
  if (f) {
    fclose(f);
  }
  if (!ok) {
    unlink(tmp_path);
  }
  free(json);
  return ok;
}

static int save_system_config_number(struct ui_state *ui, const char *key, long value) {
  char literal[64];

  snprintf(literal, sizeof(literal), "%ld", value);
  return replace_json_key_value_atomic(ui->system_config_path, key, literal);
}

static int save_system_config_string(struct ui_state *ui, const char *key,
                                     const char *value) {
  char literal[512];

  if (!append_json_string_literal(literal, sizeof(literal), value)) {
    return 0;
  }
  return replace_json_key_value_atomic(ui->system_config_path, key, literal);
}

static int ensure_os_timezone_backup(struct ui_state *ui) {
  char backup_dir[PATH_MAX];
  char backup_system_dir[PATH_MAX];
  char backup_path[PATH_MAX];
  char backup_tmp_path[PATH_MAX];
  char *data;
  size_t data_size = 0;
  int ok;

  if (!ui ||
      !join_path(backup_dir, sizeof(backup_dir), ui->plumos_root, "backups") ||
      !join_path(backup_system_dir, sizeof(backup_system_dir), backup_dir, "system") ||
      !join_path(backup_path, sizeof(backup_path), backup_system_dir, "etc-TZ.stock") ||
      snprintf(backup_tmp_path, sizeof(backup_tmp_path), "%s.tmp", backup_path) >=
          (int)sizeof(backup_tmp_path)) {
    return 0;
  }
  if (file_exists(backup_path)) {
    return 1;
  }
  mkdir(backup_dir, 0755);
  mkdir(backup_system_dir, 0755);
  data = read_file("/etc/TZ", &data_size);
  if (!data) {
    data = strdup("missing\n");
    data_size = strlen(data ? data : "");
  }
  if (!data) {
    return 0;
  }
  ok = write_buffer_atomic(backup_path, backup_tmp_path, data, data_size);
  free(data);
  return ok;
}

static int write_os_timezone_file(struct ui_state *ui, const char *timezone) {
  char value[96];

  if (!valid_timezone_value(timezone)) {
    return 0;
  }
  if (snprintf(value, sizeof(value), "%s\n", timezone) >= (int)sizeof(value)) {
    return 0;
  }
  ensure_os_timezone_backup(ui);
  return write_text_file("/etc/TZ", value);
}

static int apply_system_timezone_runtime(struct ui_state *ui, const char *timezone,
                                         char *status, size_t status_size) {
  const char *value = valid_timezone_value(timezone) ? timezone : plumos_default_timezone();
  int wrote_etc_tz;

  apply_plumos_timezone_value(value);
  wrote_etc_tz = write_os_timezone_file(ui, value);
  if (status && status_size > 0) {
    snprintf(status, status_size, wrote_etc_tz ? "OS TZ=%s" : "TZ env=%s; /etc/TZ unchanged",
             value);
  }
  return wrote_etc_tz;
}

static void init_device_settings(struct device_settings *device) {
  memset(device, 0, sizeof(*device));
  device->volume = 14;
  device->brightness = 10;
  device->lumination = 5;
  device->contrast = 10;
  device->hue = 10;
  device->saturation = 10;
  copy_string(device->language, sizeof(device->language), "en.lang");
  copy_string(device->theme, sizeof(device->theme), "default");
  copy_string(device->timezone, sizeof(device->timezone), plumos_default_timezone());
  copy_string(device->model, sizeof(device->model), "Miyoo A30");
  copy_string(device->kernel_version, sizeof(device->kernel_version), "unknown");
  copy_string(device->sdcard_storage, sizeof(device->sdcard_storage), "unavailable");
  copy_string(device->network_status_source, sizeof(device->network_status_source),
              "runtime status missing");
  copy_string(device->network_control_status, sizeof(device->network_control_status),
              "plumOS runtime control");
  copy_string(device->ssh_status, sizeof(device->ssh_status), "Not Installed");
  copy_string(device->ftp_status, sizeof(device->ftp_status), "Not Installed");
  copy_string(device->sftp_status, sizeof(device->sftp_status), "Not Installed");
  copy_string(device->samba_status, sizeof(device->samba_status), "Not Installed");
  copy_string(device->brightness_backend, sizeof(device->brightness_backend),
              "runtime backend unknown");
  copy_string(device->volume_backend, sizeof(device->volume_backend),
              "runtime backend unknown");
  copy_string(device->status, sizeof(device->status), "plumOS defaults");
}

static void load_device_runtime_status(struct ui_state *ui) {
  struct device_settings *device = &ui->device;

  device->wifi_runtime_enabled = runtime_wifi_enabled();
  copy_string(device->network_status_source, sizeof(device->network_status_source),
              file_exists(ui->wpa_status_path) ? ui->wpa_status_path : "runtime status missing");
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
  read_network_service_status(ui, "ssh", device->ssh_status,
                              sizeof(device->ssh_status),
                              &device->ssh_service_running);
  read_network_service_status(ui, "ftp", device->ftp_status,
                              sizeof(device->ftp_status),
                              &device->ftp_service_running);
  read_network_service_status(ui, "sftp", device->sftp_status,
                              sizeof(device->sftp_status),
                              &device->sftp_service_running);
  read_network_service_status(ui, "samba", device->samba_status,
                              sizeof(device->samba_status),
                              &device->samba_service_running);
}

static int load_device_settings(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *json_end;

  init_device_settings(&ui->device);
  read_first_line_file("/proc/sys/kernel/osrelease", ui->device.kernel_version,
                       sizeof(ui->device.kernel_version));
  format_storage_status(ui->sdcard_root, ui->device.sdcard_storage,
                        sizeof(ui->device.sdcard_storage));

  update_device_backend_status(&ui->device);

  load_device_runtime_status(ui);

  json = read_file(ui->system_config_path, &json_size);
  if (!json) {
    copy_string(ui->device.status, sizeof(ui->device.status), "plumOS config missing; defaults active");
    apply_system_timezone_runtime(ui, ui->device.timezone, NULL, 0);
    return 1;
  }

  json_end = json + json_size;
  ui->device.loaded = 1;
  ui->device.volume = json_get_long(json, json_end, "volume", ui->device.volume);
  {
    long stored_brightness = json_get_long(json, json_end, "brightness",
                                           ui->device.brightness);
    ui->device.brightness = brightness_setting_from_stored(stored_brightness);
    if (stored_brightness != ui->device.brightness) {
      save_system_config_number(ui, "brightness", ui->device.brightness);
    }
  }
  ui->device.lumination = json_get_long(json, json_end, "lumination", ui->device.lumination);
  ui->device.contrast = json_get_long(json, json_end, "contrast", ui->device.contrast);
  ui->device.hue = json_get_long(json, json_end, "hue", ui->device.hue);
  ui->device.saturation = json_get_long(json, json_end, "saturation", ui->device.saturation);
  json_get_string(json, json_end, "language", ui->device.language,
                  sizeof(ui->device.language));
  json_get_string(json, json_end, "theme", ui->device.theme,
                  sizeof(ui->device.theme));
  json_get_string(json, json_end, "timezone", ui->device.timezone,
                  sizeof(ui->device.timezone));
  if (!valid_timezone_value(ui->device.timezone)) {
    copy_string(ui->device.timezone, sizeof(ui->device.timezone),
                plumos_default_timezone());
  }
  apply_system_timezone_runtime(ui, ui->device.timezone, NULL, 0);
  copy_string(ui->device.status, sizeof(ui->device.status), "plumOS config loaded");
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
  copy_string(theme->id, sizeof(theme->id), theme_id && theme_id[0] ? theme_id : "default");
  copy_string(theme->target, sizeof(theme->target), "graphic");
  copy_string(theme->display_name, sizeof(theme->display_name), "Built-in Graphic");
  copy_string(theme->layout_preset, sizeof(theme->layout_preset), "grid_preview");
  copy_string(theme->font_fallback, sizeof(theme->font_fallback), "builtin");
  copy_string(theme->system_logo_root, sizeof(theme->system_logo_root), "logos/systems");
  copy_string(theme->graphic_top_layout, sizeof(theme->graphic_top_layout),
              "tile_grid");
  copy_string(theme->graphic_transition, sizeof(theme->graphic_transition), "none");
  copy_string(theme->graphic_transition_axis, sizeof(theme->graphic_transition_axis),
              "vertical");
  copy_string(theme->graphic_transition_easing, sizeof(theme->graphic_transition_easing),
              "smoothstep");
  theme->graphic_transition_ms = 0;
  copy_string(theme->status, sizeof(theme->status), "builtin graphic fallback");
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

static int valid_graphic_top_layout_value(const char *value) {
  return value && (strcmp(value, "tile_grid") == 0 ||
                   strcmp(value, "tile_strip") == 0);
}

static int valid_graphic_transition_value(const char *value) {
  return value && (strcmp(value, "slide") == 0 ||
                   strcmp(value, "none") == 0);
}

static int valid_graphic_transition_axis_value(const char *value) {
  return value && (strcmp(value, "vertical") == 0 ||
                   strcmp(value, "horizontal") == 0);
}

static int valid_graphic_transition_easing_value(const char *value) {
  return value && (strcmp(value, "smoothstep") == 0 ||
                   strcmp(value, "linear") == 0);
}

static long clamp_graphic_transition_ms(long value) {
  if (value < 80) {
    return 80;
  }
  if (value > 1000) {
    return 1000;
  }
  return value;
}

static void apply_theme_setting_overrides(struct theme_state *theme,
                                          const struct frontend_settings *settings) {
  if (!theme || !settings) {
    return;
  }
  if (valid_graphic_top_layout_value(settings->graphic_top_layout)) {
    copy_string(theme->graphic_top_layout, sizeof(theme->graphic_top_layout),
                settings->graphic_top_layout);
  }
  if (valid_graphic_transition_value(settings->graphic_transition)) {
    copy_string(theme->graphic_transition, sizeof(theme->graphic_transition),
                settings->graphic_transition);
  }
  if (valid_graphic_transition_axis_value(settings->graphic_transition_axis)) {
    copy_string(theme->graphic_transition_axis,
                sizeof(theme->graphic_transition_axis),
                settings->graphic_transition_axis);
  }
  if (valid_graphic_transition_easing_value(settings->graphic_transition_easing)) {
    copy_string(theme->graphic_transition_easing,
                sizeof(theme->graphic_transition_easing),
                settings->graphic_transition_easing);
  }
  if (settings->graphic_transition_ms > 0) {
    theme->graphic_transition_ms =
        clamp_graphic_transition_ms(settings->graphic_transition_ms);
  }
}

static int load_theme_state(struct ui_state *ui, const char *theme_id) {
  char theme_path[PATH_MAX];
  char *json;
  size_t json_size;
  const char *assets_start;
  const char *assets_end;
  const char *colors_start;
  const char *colors_end;
  const char *graphic_start;
  const char *graphic_end;
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
  json_get_string(json, json_end, "target", ui->theme.target,
                  sizeof(ui->theme.target));
  json_get_string(json, json_end, "display_name", ui->theme.display_name,
                  sizeof(ui->theme.display_name));
  json_get_string(json, json_end, "layout_preset", ui->theme.layout_preset,
                  sizeof(ui->theme.layout_preset));
  if (!ui->theme.display_name[0]) {
    copy_string(ui->theme.display_name, sizeof(ui->theme.display_name), ui->theme.id);
  }
  if (!ui->theme.layout_preset[0]) {
    copy_string(ui->theme.layout_preset, sizeof(ui->theme.layout_preset), "grid_preview");
  }

  if (ui->theme.target[0] && strcmp(ui->theme.target, "graphic") != 0) {
    ui->theme.fallback = 1;
    copy_string(ui->theme.status, sizeof(ui->theme.status),
                "theme target is not graphic; builtin fallback");
  }

  if (json_find_object(json, json_end, "assets", &assets_start, &assets_end)) {
    json_get_string(assets_start, assets_end, "font_ui", ui->theme.font_ui,
                    sizeof(ui->theme.font_ui));
    json_get_string(assets_start, assets_end, "font_fallback", ui->theme.font_fallback,
                    sizeof(ui->theme.font_fallback));
    json_get_string(assets_start, assets_end, "background", ui->theme.background,
                    sizeof(ui->theme.background));
    json_get_string(assets_start, assets_end, "gallery_background",
                    ui->theme.gallery_background,
                    sizeof(ui->theme.gallery_background));
    json_get_string(assets_start, assets_end, "system_logo_root",
                    ui->theme.system_logo_root,
                    sizeof(ui->theme.system_logo_root));
    json_get_string(assets_start, assets_end, "placeholder_thumbnail",
                    ui->theme.placeholder_thumbnail,
                    sizeof(ui->theme.placeholder_thumbnail));
  }
  if (!ui->theme.font_fallback[0]) {
    copy_string(ui->theme.font_fallback, sizeof(ui->theme.font_fallback), "builtin");
  }
  if (!ui->theme.system_logo_root[0]) {
    copy_string(ui->theme.system_logo_root, sizeof(ui->theme.system_logo_root),
                "logos/systems");
  }

  if (json_find_object(json, json_end, "colors", &colors_start, &colors_end)) {
    json_get_string(colors_start, colors_end, "background",
                    ui->theme.color_background, sizeof(ui->theme.color_background));
    json_get_string(colors_start, colors_end, "foreground",
                    ui->theme.color_foreground, sizeof(ui->theme.color_foreground));
    json_get_string(colors_start, colors_end, "muted",
                    ui->theme.color_muted, sizeof(ui->theme.color_muted));
    json_get_string(colors_start, colors_end, "accent",
                    ui->theme.color_accent, sizeof(ui->theme.color_accent));
    json_get_string(colors_start, colors_end, "panel",
                    ui->theme.color_panel, sizeof(ui->theme.color_panel));
    json_get_string(colors_start, colors_end, "panel_inner",
                    ui->theme.color_panel_inner, sizeof(ui->theme.color_panel_inner));
    json_get_string(colors_start, colors_end, "media_panel",
                    ui->theme.color_media_panel, sizeof(ui->theme.color_media_panel));
    json_get_string(colors_start, colors_end, "selection_background",
                    ui->theme.color_selection_background,
                    sizeof(ui->theme.color_selection_background));
    json_get_string(colors_start, colors_end, "selection_foreground",
                    ui->theme.color_selection_foreground,
                    sizeof(ui->theme.color_selection_foreground));
    json_get_string(colors_start, colors_end, "danger",
                    ui->theme.color_danger, sizeof(ui->theme.color_danger));
  }
  if (json_find_object(json, json_end, "graphic_mode", &graphic_start, &graphic_end)) {
    json_get_string(graphic_start, graphic_end, "top_layout",
                    ui->theme.graphic_top_layout,
                    sizeof(ui->theme.graphic_top_layout));
    json_get_string(graphic_start, graphic_end, "transition",
                    ui->theme.graphic_transition,
                    sizeof(ui->theme.graphic_transition));
    json_get_string(graphic_start, graphic_end, "transition_axis",
                    ui->theme.graphic_transition_axis,
                    sizeof(ui->theme.graphic_transition_axis));
    json_get_string(graphic_start, graphic_end, "transition_easing",
                    ui->theme.graphic_transition_easing,
                    sizeof(ui->theme.graphic_transition_easing));
    ui->theme.graphic_transition_ms =
        json_get_long(graphic_start, graphic_end, "transition_ms",
                      ui->theme.graphic_transition_ms);
  }
  if (!ui->theme.graphic_transition[0]) {
    copy_string(ui->theme.graphic_transition,
                sizeof(ui->theme.graphic_transition), "none");
  }
  if (!valid_graphic_transition_value(ui->theme.graphic_transition)) {
    copy_string(ui->theme.graphic_transition,
                sizeof(ui->theme.graphic_transition), "none");
  }
  if (!valid_graphic_top_layout_value(ui->theme.graphic_top_layout)) {
    copy_string(ui->theme.graphic_top_layout,
                sizeof(ui->theme.graphic_top_layout), "tile_grid");
  }
  if (!valid_graphic_transition_axis_value(ui->theme.graphic_transition_axis)) {
    copy_string(ui->theme.graphic_transition_axis,
                sizeof(ui->theme.graphic_transition_axis), "vertical");
  }
  if (!valid_graphic_transition_easing_value(ui->theme.graphic_transition_easing)) {
    copy_string(ui->theme.graphic_transition_easing,
                sizeof(ui->theme.graphic_transition_easing), "smoothstep");
  }
  if (ui->theme.graphic_transition_ms < 0) {
    ui->theme.graphic_transition_ms = 0;
  } else if (ui->theme.graphic_transition_ms > 1000) {
    ui->theme.graphic_transition_ms = 1000;
  }

  if (theme_behavior_is_blocked(json, json_end)) {
    ui->theme.fallback = 1;
    copy_string(ui->theme.status, sizeof(ui->theme.status),
                "theme requested behavior control; blocked");
  } else if (!ui->theme.status[0] || strcmp(ui->theme.status, "builtin graphic fallback") == 0) {
    copy_string(ui->theme.status, sizeof(ui->theme.status),
                ui->theme.font_ui[0] ? "graphic theme loaded; font available"
                                     : "graphic theme loaded; builtin font fallback");
  }

  free(json);
  return 1;
}

static int resolve_theme_font_path(const struct theme_state *theme, char *out,
                                   size_t out_size) {
  char theme_dir[PATH_MAX];
  char candidate[PATH_MAX];

  if (!theme || !theme->font_ui[0] || !out || out_size == 0) {
    return 0;
  }
  if (theme->font_ui[0] == '/') {
    if (file_exists(theme->font_ui)) {
      return copy_string(out, out_size, theme->font_ui);
    }
    return 0;
  }
  if (!dirname_path(theme_dir, sizeof(theme_dir), theme->path)) {
    return 0;
  }
  if (join_path(candidate, sizeof(candidate), theme_dir, theme->font_ui) &&
      file_exists(candidate)) {
    return copy_string(out, out_size, candidate);
  }
  return 0;
}

static int resolve_theme_asset_path(const struct theme_state *theme, const char *asset,
                                    char *out, size_t out_size) {
  char theme_dir[PATH_MAX];
  char candidate[PATH_MAX];

  if (!theme || !asset || !asset[0] || !out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (asset[0] == '/') {
    if (file_exists(asset)) {
      return copy_string(out, out_size, asset);
    }
    return 0;
  }
  if (!dirname_path(theme_dir, sizeof(theme_dir), theme->path)) {
    return 0;
  }
  if (join_path(candidate, sizeof(candidate), theme_dir, asset) &&
      file_exists(candidate)) {
    return copy_string(out, out_size, candidate);
  }
  return 0;
}

static int resolve_theme_system_logo_path(const struct ui_state *ui,
                                          const char *system_id,
                                          char *out, size_t out_size) {
  char asset[UI_PATH_MAX];
  const char *root;

  if (!ui || !system_id || !valid_system_id(system_id) ||
      !out || out_size == 0 || !ui->theme.loaded || ui->theme.fallback) {
    return 0;
  }
  out[0] = '\0';
  root = ui->theme.system_logo_root[0] ? ui->theme.system_logo_root : "logos/systems";
  if (snprintf(asset, sizeof(asset), "%s/%s.png", root, system_id) >=
      (int)sizeof(asset)) {
    return 0;
  }
  return resolve_theme_asset_path(&ui->theme, asset, out, out_size);
}

static int graphic_theme_choice_cmp(const void *a, const void *b) {
  const struct graphic_theme_choice *theme_a = (const struct graphic_theme_choice *)a;
  const struct graphic_theme_choice *theme_b = (const struct graphic_theme_choice *)b;

  if (strcmp(theme_a->raw, "default") == 0 && strcmp(theme_b->raw, "default") != 0) {
    return -1;
  }
  if (strcmp(theme_b->raw, "default") == 0 && strcmp(theme_a->raw, "default") != 0) {
    return 1;
  }
  return strcmp(theme_a->display, theme_b->display);
}

static int graphic_theme_choice_exists(const struct graphic_theme_choice *choices,
                                       size_t count, const char *id) {
  size_t i;

  if (!choices || !id) {
    return 0;
  }
  for (i = 0; i < count; i++) {
    if (strcmp(choices[i].raw, id) == 0) {
      return 1;
    }
  }
  return 0;
}

static int read_graphic_theme_display_name(const char *theme_path, const char *theme_id,
                                           int allow_builtin_fallback,
                                           char *display, size_t display_size) {
  char *json;
  size_t json_size;
  char target[32];

  if (!display || display_size == 0 || !theme_id || !theme_id[0]) {
    return 0;
  }
  display[0] = '\0';
  json = read_file(theme_path, &json_size);
  if (!json) {
    if (allow_builtin_fallback && strcmp(theme_id, "default") == 0) {
      return copy_string(display, display_size, "Default Graphic");
    }
    return 0;
  }
  target[0] = '\0';
  json_get_string(json, json + json_size, "target", target, sizeof(target));
  if (target[0] && strcmp(target, "graphic") != 0) {
    free(json);
    return 0;
  }
  if (theme_behavior_is_blocked(json, json + json_size)) {
    free(json);
    return 0;
  }
  json_get_string(json, json + json_size, "display_name", display, display_size);
  free(json);
  if (!display[0]) {
    return copy_string(display, display_size, theme_id);
  }
  return 1;
}

static int append_graphic_theme_choice(struct ui_state *ui,
                                       struct graphic_theme_choice *choices,
                                       size_t capacity, size_t *count,
                                       const char *theme_id,
                                       int allow_builtin_fallback) {
  char theme_path[PATH_MAX];
  char display[128];

  if (!ui || !choices || !count || *count >= capacity ||
      !theme_id || !valid_system_id(theme_id) ||
      graphic_theme_choice_exists(choices, *count, theme_id) ||
      !build_theme_path(theme_path, sizeof(theme_path), ui->plumos_root, theme_id)) {
    return 0;
  }
  if (!read_graphic_theme_display_name(theme_path, theme_id,
                                       allow_builtin_fallback,
                                       display, sizeof(display))) {
    return 0;
  }
  copy_string(choices[*count].raw, sizeof(choices[*count].raw), theme_id);
  copy_string(choices[*count].display, sizeof(choices[*count].display), display);
  (*count)++;
  return 1;
}

static size_t load_graphic_theme_choices(struct ui_state *ui,
                                         struct graphic_theme_choice *choices,
                                         size_t capacity) {
  char themes_dir[PATH_MAX];
  DIR *dir;
  struct dirent *entry;
  size_t count = 0;

  if (!ui || !choices || capacity == 0) {
    return 0;
  }
  append_graphic_theme_choice(ui, choices, capacity, &count, "default", 1);
  if (!join_path(themes_dir, sizeof(themes_dir), ui->plumos_root, "themes")) {
    return count;
  }
  dir = opendir(themes_dir);
  if (!dir) {
    return count;
  }
  while ((entry = readdir(dir)) != NULL && count < capacity) {
    const char *name = entry->d_name;
    char theme_path[PATH_MAX];
    struct stat st;

    if (!name || name[0] == '.' || !valid_system_id(name) ||
        strcmp(name, "default") == 0 ||
        !build_theme_path(theme_path, sizeof(theme_path), ui->plumos_root, name) ||
        stat(theme_path, &st) != 0 || !S_ISREG(st.st_mode)) {
      continue;
    }
    append_graphic_theme_choice(ui, choices, capacity, &count, name, 0);
  }
  closedir(dir);
  if (count > 1) {
    qsort(choices, count, sizeof(choices[0]), graphic_theme_choice_cmp);
  }
  return count;
}

static void graphic_theme_display_value(struct ui_state *ui, const char *raw_value,
                                        char *out, size_t out_size) {
  struct graphic_theme_choice choices[UI_GRAPHIC_THEME_CHOICE_MAX];
  size_t count;
  size_t i;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!raw_value || !raw_value[0]) {
    raw_value = "default";
  }
  count = load_graphic_theme_choices(ui, choices,
                                     sizeof(choices) / sizeof(choices[0]));
  for (i = 0; i < count; i++) {
    if (strcmp(choices[i].raw, raw_value) == 0) {
      copy_string(out, out_size, choices[i].display);
      return;
    }
  }
  copy_string(out, out_size, raw_value);
}

static int font_path_is_bitmap_only(const char *path) {
  const char *ext;

  if (!path) {
    return 0;
  }
  ext = strrchr(path, '.');
  return ext && (strcmp(ext, ".bdf") == 0 || strcmp(ext, ".pcf") == 0);
}

static int choose_mali_font_path(struct ui_state *ui, const char *requested,
                                 char *out, size_t out_size) {
  static const char *rel_candidates[] = {
      "plumos/fonts/ui.ttf",
      "plumos/fonts/default.ttf",
      "plumos/fonts/default.otf",
      "RetroArch/.retroarch/assets/pkg/chinese-fallback-font.ttf",
      "RetroArch/.retroarch/system/msyh.ttf",
      "miyoo/res/wqy-microhei.ttc",
      "miyoo/res/MicrosoftYaHeiGB.ttf",
      "App/commander/res/wqy-microhei.ttc",
      "Themes/MakoVII/wqy-microhei.ttf",
  };
  size_t i;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  if (requested && requested[0]) {
    if (file_exists(requested)) {
      return copy_string(out, out_size, requested);
    }
    return copy_string(out, out_size, requested);
  }
  if (ui && strcmp(ui->frontend_settings.ui_mode, "graphic") == 0 &&
      resolve_theme_font_path(&ui->theme, out, out_size) &&
      !font_path_is_bitmap_only(out)) {
    return 1;
  }
  out[0] = '\0';
  for (i = 0; i < sizeof(rel_candidates) / sizeof(rel_candidates[0]); i++) {
    char candidate[PATH_MAX];
    if (join_path(candidate, sizeof(candidate), ui->sdcard_root, rel_candidates[i]) &&
        file_exists(candidate)) {
      return copy_string(out, out_size, candidate);
    }
  }
  return 0;
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

static const struct setting_choice UI_MODE_CHOICES[] = {
    {"text", "Text"},
    {"graphic", "Graphic"},
};

static const struct setting_choice BOOT_RESUME_CHOICES[] = {
    {"off", "Off"},
    {"last", "Last"},
    {"picker", "Picker"},
};

static const struct setting_choice SORT_SYSTEMS_CHOICES[] = {
    {"sort_order", "Sort Order"},
    {"name", "Name"},
};

static const struct setting_choice SORT_ROMS_CHOICES[] = {
    {"name", "Name"},
    {"path", "Path"},
};

static const struct setting_choice GRAPHIC_TOP_LAYOUT_CHOICES[] = {
    {"tile_grid", "Grid 3x2"},
    {"tile_strip", "Strip 2x1"},
};

static const struct setting_choice GRAPHIC_TRANSITION_CHOICES[] = {
    {"none", "None"},
    {"slide", "Slide"},
};

static const struct setting_choice GRAPHIC_TRANSITION_AXIS_CHOICES[] = {
    {"vertical", "Vertical"},
    {"horizontal", "Horizontal"},
};

static const struct setting_choice GRAPHIC_TRANSITION_EASING_CHOICES[] = {
    {"smoothstep", "Smooth"},
    {"linear", "Linear"},
};

static const struct setting_choice SYSTEM_LANGUAGE_CHOICES[] = {
    {"en.lang", "English"},
    {"ja.lang", "Japanese"},
    {"ch.lang", "Chinese"},
    {"cht.lang", "Traditional Chinese"},
    {"ko.lang", "Korean"},
    {"es.lang", "Spanish"},
    {"pt.lang", "Portuguese"},
};

static const struct setting_choice SYSTEM_TIMEZONE_CHOICES[] = {
    {"JST-9", "Japan"},
    {"UTC0", "UTC"},
    {"KST-9", "Korea"},
    {"CST-8", "China"},
    {"PST8PDT,M3.2.0/2,M11.1.0/2", "US Pacific"},
    {"MST7MDT,M3.2.0/2,M11.1.0/2", "US Mountain"},
    {"CST6CDT,M3.2.0/2,M11.1.0/2", "US Central"},
    {"EST5EDT,M3.2.0/2,M11.1.0/2", "US Eastern"},
    {"GMT0BST,M3.5.0/1,M10.5.0", "UK"},
    {"CET-1CEST,M3.5.0/2,M10.5.0/3", "Central EU"},
};

static const long BRIGHTNESS_TEST_VALUES[] = {
    10, 30, 50, 70, 90, 110, 130, 150, 170, 190, 210, 230, 250, 255,
};
#define BRIGHTNESS_TEST_COUNT \
  (sizeof(BRIGHTNESS_TEST_VALUES) / sizeof(BRIGHTNESS_TEST_VALUES[0]))
#define BRIGHTNESS_TEST_COLUMNS 4

enum setting_control_type {
  SETTING_CONTROL_READONLY = 0,
  SETTING_CONTROL_CHECKBOX,
  SETTING_CONTROL_CHOICE,
  SETTING_CONTROL_NUMBER,
  SETTING_CONTROL_ACTION
};

static const struct setting_choice *setting_choices(const char *id, size_t *count_out) {
  if (count_out) {
    *count_out = 0;
  }
  if (!id) {
    return NULL;
  }
  if (strcmp(id, "ui_mode") == 0) {
    if (count_out) {
      *count_out = sizeof(UI_MODE_CHOICES) / sizeof(UI_MODE_CHOICES[0]);
    }
    return UI_MODE_CHOICES;
  }
  if (strcmp(id, "boot_resume_mode") == 0) {
    if (count_out) {
      *count_out = sizeof(BOOT_RESUME_CHOICES) / sizeof(BOOT_RESUME_CHOICES[0]);
    }
    return BOOT_RESUME_CHOICES;
  }
  if (strcmp(id, "sort_systems") == 0) {
    if (count_out) {
      *count_out = sizeof(SORT_SYSTEMS_CHOICES) / sizeof(SORT_SYSTEMS_CHOICES[0]);
    }
    return SORT_SYSTEMS_CHOICES;
  }
  if (strcmp(id, "sort_roms") == 0) {
    if (count_out) {
      *count_out = sizeof(SORT_ROMS_CHOICES) / sizeof(SORT_ROMS_CHOICES[0]);
    }
    return SORT_ROMS_CHOICES;
  }
  if (strcmp(id, "theme_top_layout") == 0) {
    if (count_out) {
      *count_out = sizeof(GRAPHIC_TOP_LAYOUT_CHOICES) /
                   sizeof(GRAPHIC_TOP_LAYOUT_CHOICES[0]);
    }
    return GRAPHIC_TOP_LAYOUT_CHOICES;
  }
  if (strcmp(id, "theme_transition") == 0) {
    if (count_out) {
      *count_out = sizeof(GRAPHIC_TRANSITION_CHOICES) /
                   sizeof(GRAPHIC_TRANSITION_CHOICES[0]);
    }
    return GRAPHIC_TRANSITION_CHOICES;
  }
  if (strcmp(id, "theme_transition_axis") == 0) {
    if (count_out) {
      *count_out = sizeof(GRAPHIC_TRANSITION_AXIS_CHOICES) /
                   sizeof(GRAPHIC_TRANSITION_AXIS_CHOICES[0]);
    }
    return GRAPHIC_TRANSITION_AXIS_CHOICES;
  }
  if (strcmp(id, "theme_transition_easing") == 0) {
    if (count_out) {
      *count_out = sizeof(GRAPHIC_TRANSITION_EASING_CHOICES) /
                   sizeof(GRAPHIC_TRANSITION_EASING_CHOICES[0]);
    }
    return GRAPHIC_TRANSITION_EASING_CHOICES;
  }
  if (strcmp(id, "system_language") == 0) {
    if (count_out) {
      *count_out = sizeof(SYSTEM_LANGUAGE_CHOICES) / sizeof(SYSTEM_LANGUAGE_CHOICES[0]);
    }
    return SYSTEM_LANGUAGE_CHOICES;
  }
  if (strcmp(id, "system_timezone") == 0) {
    if (count_out) {
      *count_out = sizeof(SYSTEM_TIMEZONE_CHOICES) / sizeof(SYSTEM_TIMEZONE_CHOICES[0]);
    }
    return SYSTEM_TIMEZONE_CHOICES;
  }
  return NULL;
}

static const char *setting_choice_display_value(const char *id, const char *raw_value) {
  size_t count = 0;
  size_t i;
  const struct setting_choice *choices = setting_choices(id, &count);

  if (!raw_value || !raw_value[0]) {
    raw_value = "";
  }
  for (i = 0; i < count; i++) {
    if (strcmp(raw_value, choices[i].raw) == 0 ||
        strcmp(raw_value, choices[i].display) == 0) {
      return choices[i].display;
    }
  }
  return raw_value[0] ? raw_value : "-";
}

static int rom_scan_policy_is_on_enter(const char *policy) {
  return !policy || !policy[0] || strcmp(policy, "on_enter") == 0 ||
         strcmp(policy, "On Enter") == 0;
}

static int compare_text_ci(const char *a, const char *b) {
  const unsigned char *pa = (const unsigned char *)(a ? a : "");
  const unsigned char *pb = (const unsigned char *)(b ? b : "");

  while (*pa && *pb) {
    int ca = tolower(*pa);
    int cb = tolower(*pb);
    if (ca != cb) {
      return ca - cb;
    }
    pa++;
    pb++;
  }
  return (int)*pa - (int)*pb;
}

static int cmp_top_entry_name(const void *a, const void *b) {
  const struct top_entry *ea = (const struct top_entry *)a;
  const struct top_entry *eb = (const struct top_entry *)b;
  int cmp = compare_text_ci(ea->display_name, eb->display_name);

  if (cmp != 0) {
    return cmp;
  }
  return compare_text_ci(ea->id, eb->id);
}

static int cmp_rom_entry_name(const void *a, const void *b) {
  const struct rom_entry *ea = (const struct rom_entry *)a;
  const struct rom_entry *eb = (const struct rom_entry *)b;
  int cmp = compare_text_ci(ea->title, eb->title);

  if (cmp != 0) {
    return cmp;
  }
  return compare_text_ci(ea->relative_path, eb->relative_path);
}

static int cmp_rom_entry_path(const void *a, const void *b) {
  const struct rom_entry *ea = (const struct rom_entry *)a;
  const struct rom_entry *eb = (const struct rom_entry *)b;
  int cmp = compare_text_ci(ea->relative_path, eb->relative_path);

  if (cmp != 0) {
    return cmp;
  }
  return compare_text_ci(ea->title, eb->title);
}

static enum setting_control_type setting_control_type_for_id(const char *id) {
  if (!id) {
    return SETTING_CONTROL_READONLY;
  }
  if (strcmp(id, "network_connect_wifi") == 0 ||
      strcmp(id, "network_services") == 0 ||
      strcmp(id, "network_usb_disk_mode") == 0 ||
      strcmp(id, "network_information") == 0 ||
      strcmp(id, "ui_theme_settings") == 0 ||
      strcmp(id, "system_display_color") == 0 ||
      strcmp(id, "system_time_settings") == 0 ||
      strcmp(id, "system_manual_time") == 0 ||
      strcmp(id, "system_manual_time_apply") == 0 ||
      strcmp(id, "system_information") == 0 ||
      strcmp(id, "performance_clear_cpu_override") == 0 ||
      strcmp(id, "performance_core_details") == 0) {
    return SETTING_CONTROL_ACTION;
  }
  if (strcmp(id, "show_empty_systems") == 0 ||
      strcmp(id, "show_favorites_on_top") == 0 ||
      strcmp(id, "show_recent_on_top") == 0 ||
      strcmp(id, "rom_scan_policy") == 0 ||
      strcmp(id, "network_wifi_enabled") == 0 ||
      strcmp(id, "network_ssh_enabled") == 0 ||
      strcmp(id, "network_ftp_enabled") == 0 ||
      strcmp(id, "network_sftp_enabled") == 0 ||
      strcmp(id, "network_samba_enabled") == 0) {
    return SETTING_CONTROL_CHECKBOX;
  }
  if (setting_choices(id, NULL)) {
    return SETTING_CONTROL_CHOICE;
  }
  if (strcmp(id, "graphic_theme_id") == 0) {
    return SETTING_CONTROL_CHOICE;
  }
  if (strcmp(id, "performance_system") == 0 ||
      strcmp(id, "performance_cpu_policy") == 0 ||
      strcmp(id, "performance_cpu_cores") == 0) {
    return SETTING_CONTROL_CHOICE;
  }
  if (strcmp(id, "rom_scan_slow_threshold_ms") == 0 ||
      strcmp(id, "rom_scan_test_file_count") == 0 ||
      strcmp(id, "system_volume") == 0 ||
      strcmp(id, "system_brightness") == 0 ||
      strcmp(id, "system_lumination") == 0 ||
      strcmp(id, "system_contrast") == 0 ||
      strcmp(id, "system_hue") == 0 ||
      strcmp(id, "system_saturation") == 0 ||
      strcmp(id, "system_manual_time_year") == 0 ||
      strcmp(id, "system_manual_time_month") == 0 ||
      strcmp(id, "system_manual_time_day") == 0 ||
      strcmp(id, "system_manual_time_hour") == 0 ||
      strcmp(id, "system_manual_time_minute") == 0 ||
      strcmp(id, "theme_transition_ms") == 0) {
    return SETTING_CONTROL_NUMBER;
  }
  return SETTING_CONTROL_READONLY;
}

static int setting_is_writable(const char *id) {
  return id && (strcmp(id, "ui_mode") == 0 ||
                strcmp(id, "show_empty_systems") == 0 ||
                strcmp(id, "show_favorites_on_top") == 0 ||
                strcmp(id, "show_recent_on_top") == 0 ||
                strcmp(id, "boot_resume_mode") == 0 ||
                strcmp(id, "graphic_theme_id") == 0 ||
                strcmp(id, "theme_top_layout") == 0 ||
                strcmp(id, "theme_transition") == 0 ||
                strcmp(id, "theme_transition_axis") == 0 ||
                strcmp(id, "theme_transition_easing") == 0 ||
                strcmp(id, "theme_transition_ms") == 0 ||
                strcmp(id, "sort_systems") == 0 ||
                strcmp(id, "sort_roms") == 0 ||
                strcmp(id, "rom_scan_policy") == 0 ||
                strcmp(id, "rom_scan_slow_threshold_ms") == 0 ||
                strcmp(id, "rom_scan_test_file_count") == 0 ||
                strcmp(id, "system_volume") == 0 ||
                strcmp(id, "system_brightness") == 0 ||
                strcmp(id, "system_lumination") == 0 ||
                strcmp(id, "system_contrast") == 0 ||
                strcmp(id, "system_hue") == 0 ||
                strcmp(id, "system_saturation") == 0 ||
                strcmp(id, "system_language") == 0 ||
                strcmp(id, "system_timezone") == 0 ||
                strcmp(id, "system_manual_time_year") == 0 ||
                strcmp(id, "system_manual_time_month") == 0 ||
                strcmp(id, "system_manual_time_day") == 0 ||
                strcmp(id, "system_manual_time_hour") == 0 ||
                strcmp(id, "system_manual_time_minute") == 0 ||
                strcmp(id, "network_wifi_enabled") == 0 ||
                strcmp(id, "network_ssh_enabled") == 0 ||
                strcmp(id, "network_ftp_enabled") == 0 ||
                strcmp(id, "network_sftp_enabled") == 0 ||
                strcmp(id, "network_samba_enabled") == 0 ||
                strcmp(id, "performance_system") == 0 ||
                strcmp(id, "performance_cpu_policy") == 0 ||
                strcmp(id, "performance_cpu_cores") == 0);
}

static const char *settings_category_title(enum settings_category category) {
  switch (category) {
  case SETTINGS_CATEGORY_SYSTEM_DISPLAY_COLOR:
    return "System Settings - Display Color";
  case SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST:
    return "System Settings - Brightness Test";
  case SETTINGS_CATEGORY_SYSTEM_TIME:
    return "System Settings - Time Settings";
  case SETTINGS_CATEGORY_SYSTEM_TIME_MANUAL:
    return "System Settings - Manual Time";
  case SETTINGS_CATEGORY_SYSTEM_INFORMATION:
    return "System Settings - INFORMATION";
  case SETTINGS_CATEGORY_SYSTEM:
    return "System Settings";
  case SETTINGS_CATEGORY_NETWORK:
    return "Network Settings";
  case SETTINGS_CATEGORY_NETWORK_SERVICE:
    return "Network Settings - NW Service";
  case SETTINGS_CATEGORY_NETWORK_INFORMATION:
    return "Network Settings - INFORMATION";
  case SETTINGS_CATEGORY_PERFORMANCE:
    return "Performance Settings";
  case SETTINGS_CATEGORY_UI_THEME:
    return "Theme Settings";
  case SETTINGS_CATEGORY_UI:
  default:
    return "UI Settings";
  }
}

static void add_ui_settings_entries(struct ui_state *ui,
                                    const struct frontend_settings *settings) {
  add_setting_entry(ui, "ui_mode", "UI Mode",
                    setting_choice_display_value("ui_mode", settings->ui_mode));
  add_bool_setting_entry(ui, "show_empty_systems", "Show Empty Systems",
                         settings->show_empty_systems);
  add_bool_setting_entry(ui, "show_favorites_on_top", "Favorites On TOP",
                         settings->show_favorites_on_top);
  add_bool_setting_entry(ui, "show_recent_on_top", "Recent On TOP",
                         settings->show_recent_on_top);
  add_setting_entry(ui, "boot_resume_mode", "Boot Resume Mode",
                    setting_choice_display_value("boot_resume_mode",
                                                 settings->boot_resume_mode));
  add_setting_entry(ui, "sort_systems", "Sort Systems",
                    setting_choice_display_value("sort_systems", settings->sort_systems));
  add_setting_entry(ui, "sort_roms", "Sort ROMs",
                    setting_choice_display_value("sort_roms", settings->sort_roms));
  add_bool_setting_entry(ui, "rom_scan_policy", "Scan On Enter",
                         rom_scan_policy_is_on_enter(settings->rom_scan_policy));
  add_setting_entry(ui, "ui_theme_settings", "Theme Settings", "");
}

static void add_ui_theme_settings_entries(struct ui_state *ui,
                                          const struct frontend_settings *settings) {
  char graphic_theme_display[128];
  char value[128];

  graphic_theme_display_value(ui, settings->graphic_theme_id,
                              graphic_theme_display,
                              sizeof(graphic_theme_display));
  add_setting_entry(ui, "graphic_theme_id", "Theme", graphic_theme_display);
  add_setting_entry(ui, "theme_name", "Name", ui->theme.display_name);
  add_setting_entry(ui, "theme_status", "Status", ui->theme.status);
  add_setting_entry(ui, "theme_layout", "Layout", ui->theme.layout_preset);
  add_setting_entry(ui, "theme_top_layout", "TOP Layout",
                    setting_choice_display_value("theme_top_layout",
                                                 ui->theme.graphic_top_layout));
  add_setting_entry(ui, "theme_transition", "Transition",
                    setting_choice_display_value("theme_transition",
                                                 ui->theme.graphic_transition));
  snprintf(value, sizeof(value), "%ld ms", ui->theme.graphic_transition_ms);
  add_setting_entry(ui, "theme_transition_ms", "Time", value);
  add_setting_entry(ui, "theme_transition_axis", "Axis",
                    setting_choice_display_value("theme_transition_axis",
                                                 ui->theme.graphic_transition_axis));
  add_setting_entry(ui, "theme_transition_easing", "Easing",
                    setting_choice_display_value("theme_transition_easing",
                                                 ui->theme.graphic_transition_easing));
  add_setting_entry(ui, "theme_font", "Font",
                    ui->theme.font_ui[0] ? ui->theme.font_ui : ui->theme.font_fallback);
}

static int days_in_month(long year, long month) {
  static const int days[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
  };
  int leap;

  if (month < 1 || month > 12) {
    return 31;
  }
  if (month != 2) {
    return days[month - 1];
  }
  leap = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
  return leap ? 29 : 28;
}

static void clamp_manual_time_fields(struct ui_state *ui) {
  long max_day;

  if (!ui) {
    return;
  }
  ui->manual_time_year = clamp_long(ui->manual_time_year, 2024, 2037);
  ui->manual_time_month = clamp_long(ui->manual_time_month, 1, 12);
  max_day = days_in_month(ui->manual_time_year, ui->manual_time_month);
  ui->manual_time_day = clamp_long(ui->manual_time_day, 1, max_day);
  ui->manual_time_hour = clamp_long(ui->manual_time_hour, 0, 23);
  ui->manual_time_minute = clamp_long(ui->manual_time_minute, 0, 59);
}

static void init_manual_time_from_current(struct ui_state *ui) {
  time_t now;
  struct tm tm_value;

  if (!ui) {
    return;
  }
  apply_plumos_timezone_value(ui->device.timezone);
  now = time(NULL);
  if (now == (time_t)-1 || !localtime_r(&now, &tm_value)) {
    ui->manual_time_year = 2026;
    ui->manual_time_month = 1;
    ui->manual_time_day = 1;
    ui->manual_time_hour = 0;
    ui->manual_time_minute = 0;
  } else {
    ui->manual_time_year = tm_value.tm_year + 1900;
    ui->manual_time_month = tm_value.tm_mon + 1;
    ui->manual_time_day = tm_value.tm_mday;
    ui->manual_time_hour = tm_value.tm_hour;
    ui->manual_time_minute = tm_value.tm_min;
  }
  clamp_manual_time_fields(ui);
  ui->manual_time_initialized = 1;
}

static void format_manual_time_value(const struct ui_state *ui,
                                     char *out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  if (!ui || !ui->manual_time_initialized) {
    copy_string(out, out_size, "-");
    return;
  }
  snprintf(out, out_size, "%04ld-%02ld-%02ld %02ld:%02ld",
           ui->manual_time_year, ui->manual_time_month, ui->manual_time_day,
           ui->manual_time_hour, ui->manual_time_minute);
}

static void add_system_time_entries(struct ui_state *ui) {
  char value[256];
  const struct device_settings *device = &ui->device;

  format_current_time_local(value, sizeof(value));
  add_setting_entry(ui, "system_current_time", "Current Time", value);
  add_setting_entry(ui, "system_timezone", "Timezone",
                    setting_choice_display_value("system_timezone", device->timezone));
  if (!ui->manual_time_initialized) {
    init_manual_time_from_current(ui);
  }
  format_manual_time_value(ui, value, sizeof(value));
  add_setting_entry(ui, "system_manual_time", "Manual Time", value);
}

static void add_system_time_manual_entries(struct ui_state *ui) {
  char value[256];

  if (!ui->manual_time_initialized) {
    init_manual_time_from_current(ui);
  }
  clamp_manual_time_fields(ui);
  format_current_time_local(value, sizeof(value));
  add_setting_entry(ui, "system_current_time", "Current Time", value);
  add_setting_entry(ui, "system_manual_timezone", "Timezone",
                    setting_choice_display_value("system_timezone",
                                                 ui->device.timezone));
  snprintf(value, sizeof(value), "%ld", ui->manual_time_year);
  add_setting_entry(ui, "system_manual_time_year", "Year", value);
  snprintf(value, sizeof(value), "%ld", ui->manual_time_month);
  add_setting_entry(ui, "system_manual_time_month", "Month", value);
  snprintf(value, sizeof(value), "%ld", ui->manual_time_day);
  add_setting_entry(ui, "system_manual_time_day", "Day", value);
  snprintf(value, sizeof(value), "%ld", ui->manual_time_hour);
  add_setting_entry(ui, "system_manual_time_hour", "Hour", value);
  snprintf(value, sizeof(value), "%ld", ui->manual_time_minute);
  add_setting_entry(ui, "system_manual_time_minute", "Minute", value);
  format_manual_time_value(ui, value, sizeof(value));
  add_setting_entry(ui, "system_manual_time_apply", "Apply Manual Time", value);
}

static void add_system_settings_entries(struct ui_state *ui) {
  char value[256];
  const struct device_settings *device = &ui->device;

  snprintf(value, sizeof(value), "%ld", device->volume);
  add_setting_entry(ui, "system_volume", "Volume", value);

  snprintf(value, sizeof(value), "%ld", device->brightness);
  add_setting_entry(ui, "system_brightness", "Brightness", value);
  snprintf(value, sizeof(value), "%ld", device->lumination);
  add_setting_entry(ui, "system_lumination", "Lumination", value);
  snprintf(value, sizeof(value), "C=%ld H=%ld S=%ld",
           device->contrast, device->hue, device->saturation);
  add_setting_entry(ui, "system_display_color", "Display Color", value);

  add_setting_entry(ui, "system_time_settings", "Time Settings",
                    setting_choice_display_value("system_timezone", device->timezone));
  add_setting_entry(ui, "system_language", "Language",
                    setting_choice_display_value("system_language", device->language));
  add_setting_entry(ui, "system_information", "INFORMATION", "");
}

static void add_system_brightness_test_entries(struct ui_state *ui) {
  size_t i;
  char id[64];
  char value[32];
  const struct device_settings *device = &ui->device;

  for (i = 0; i < BRIGHTNESS_TEST_COUNT; i++) {
    long brightness = BRIGHTNESS_TEST_VALUES[i];
    snprintf(id, sizeof(id), "system_brightness_test_%ld", brightness);
    snprintf(value, sizeof(value), "%ld", brightness);
    add_setting_entry(ui, id, value,
                      brightness_raw_value(device->brightness) == brightness ? "current"
                                                                             : "preset");
  }
}

static void add_system_display_color_entries(struct ui_state *ui) {
  char value[256];
  const struct device_settings *device = &ui->device;

  snprintf(value, sizeof(value), "%ld", device->contrast);
  add_setting_entry(ui, "system_contrast", "Contrast", value);
  snprintf(value, sizeof(value), "%ld", device->hue);
  add_setting_entry(ui, "system_hue", "Hue", value);
  snprintf(value, sizeof(value), "%ld", device->saturation);
  add_setting_entry(ui, "system_saturation", "Saturation", value);
}

static void add_system_information_entries(struct ui_state *ui) {
  const struct device_settings *device = &ui->device;

  add_setting_entry(ui, "system_model", "Device Model", device->model);
  add_setting_entry(ui, "system_kernel", "Linux Kernel", device->kernel_version);
  add_setting_entry(ui, "system_sdcard", "SD Card", device->sdcard_storage);
  add_setting_entry(ui, "system_config", "plumOS System Config",
                    device->loaded ? ui->system_config_path : device->status);
  add_setting_entry(ui, "system_input_device", "Input Device", ui->input_event_path);
  add_setting_entry(ui, "system_audio_backend", "Audio Backend", device->volume_backend);
  add_setting_entry(ui, "system_display_backend", "Display Backend",
                    device->brightness_backend);
  add_setting_entry(ui, "system_write_policy", "Write Policy",
                    "plumOS-only config; stockOS untouched");
}

static void add_network_settings_entries(struct ui_state *ui) {
  const struct device_settings *device = &ui->device;

  add_bool_setting_entry(ui, "network_wifi_enabled", "Wi-Fi",
                         device->wifi_runtime_enabled);
  add_setting_entry(ui, "network_connect_wifi", "Connect Wi-Fi",
                    "Scan SSID");
  add_setting_entry(ui, "network_services", "NW Service",
                    "File Transfer");
  add_setting_entry(ui, "network_information", "INFORMATION", "");
}

static void add_network_service_entries(struct ui_state *ui) {
  const struct device_settings *device = &ui->device;

  add_bool_setting_entry(ui, "network_ssh_enabled", "SSH",
                         device->ssh_service_running);
  add_bool_setting_entry(ui, "network_ftp_enabled", "FTP",
                         device->ftp_service_running);
  add_bool_setting_entry(ui, "network_sftp_enabled", "SFTP",
                         device->sftp_service_running);
  add_bool_setting_entry(ui, "network_samba_enabled", "Samba",
                         device->samba_service_running);
  add_setting_entry(ui, "network_usb_disk_mode", "USB Disk Mode",
                    "USB drive");
}

static void add_network_information_entries(struct ui_state *ui) {
  char value[256];
  const struct device_settings *device = &ui->device;

  add_setting_entry(ui, "network_connection", "Connection",
                    device->wifi_state[0] ? device->wifi_state : "No Runtime Status");
  add_setting_entry(ui, "network_ip_address", "IP Address",
                    device->wifi_ip[0] ? device->wifi_ip : "-");
  if (device->wifi_rssi[0]) {
    snprintf(value, sizeof(value), "%s dBm", device->wifi_rssi);
  } else {
    copy_string(value, sizeof(value), "-");
  }
  add_setting_entry(ui, "network_signal", "Signal", value);
  if (device->wifi_linkspeed[0]) {
    snprintf(value, sizeof(value), "%s Mbps", device->wifi_linkspeed);
  } else {
    copy_string(value, sizeof(value), "-");
  }
  add_setting_entry(ui, "network_link_speed", "Link Speed", value);
  if (device->wifi_frequency[0]) {
    snprintf(value, sizeof(value), "%s MHz", device->wifi_frequency);
  } else {
    copy_string(value, sizeof(value), "-");
  }
  add_setting_entry(ui, "network_frequency", "Frequency", value);
  add_setting_entry(ui, "network_ssh", "SSH", device->ssh_status);
  add_setting_entry(ui, "network_ftp_status", "FTP", device->ftp_status);
  add_setting_entry(ui, "network_sftp_status", "SFTP", device->sftp_status);
  add_setting_entry(ui, "network_samba_status", "Samba", device->samba_status);
}

static int performance_top_entry_is_real(const struct top_entry *entry) {
  return entry && !entry->virtual_entry && valid_system_id(entry->id);
}

static int performance_find_top_entry_index(const struct ui_state *ui,
                                            const char *system_id) {
  size_t i;

  if (!ui || !system_id || !system_id[0]) {
    return -1;
  }
  for (i = 0; i < ui->top_count; i++) {
    if (performance_top_entry_is_real(&ui->top_entries[i]) &&
        strcmp(ui->top_entries[i].id, system_id) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int performance_first_top_entry_index(const struct ui_state *ui) {
  size_t i;

  if (!ui) {
    return -1;
  }
  for (i = 0; i < ui->top_count; i++) {
    if (performance_top_entry_is_real(&ui->top_entries[i])) {
      return (int)i;
    }
  }
  return -1;
}

static void performance_select_top_entry(struct ui_state *ui, size_t index) {
  const struct top_entry *entry;

  if (!ui || index >= ui->top_count) {
    return;
  }
  entry = &ui->top_entries[index];
  copy_string(ui->performance_system_id, sizeof(ui->performance_system_id), entry->id);
  copy_string(ui->performance_system_name, sizeof(ui->performance_system_name),
              entry->display_name[0] ? entry->display_name : entry->id);
}

static int performance_ensure_system(struct ui_state *ui) {
  int index;
  int old_show_all;

  if (!ui) {
    return 0;
  }
  if (ui->top_count == 0) {
    load_top_entries(ui);
  }
  index = performance_find_top_entry_index(ui, ui->performance_system_id);
  if (index < 0) {
    index = performance_find_top_entry_index(ui, ui->current_system_id);
  }
  if (index < 0) {
    index = performance_first_top_entry_index(ui);
  }
  if (index < 0) {
    old_show_all = ui->show_all;
    ui->show_all = 1;
    load_top_entries(ui);
    ui->show_all = old_show_all;
    index = performance_first_top_entry_index(ui);
  }
  if (index < 0) {
    ui->performance_system_id[0] = '\0';
    ui->performance_system_name[0] = '\0';
    return 0;
  }
  performance_select_top_entry(ui, (size_t)index);
  return 1;
}

static int performance_cycle_system(struct ui_state *ui, int direction) {
  int index;
  size_t step;

  if (!ui || direction == 0 || !performance_ensure_system(ui)) {
    return 0;
  }
  index = performance_find_top_entry_index(ui, ui->performance_system_id);
  if (index < 0) {
    index = performance_first_top_entry_index(ui);
  }
  if (index < 0 || ui->top_count == 0) {
    return 0;
  }
  for (step = 1; step <= ui->top_count; step++) {
    int next = direction > 0
                   ? (int)(((size_t)index + step) % ui->top_count)
                   : (int)(((size_t)index + ui->top_count - (step % ui->top_count)) %
                           ui->top_count);
    if (performance_top_entry_is_real(&ui->top_entries[next])) {
      performance_select_top_entry(ui, (size_t)next);
      return 1;
    }
  }
  return 0;
}

static void copy_trimmed_range(char *out, size_t out_size, const char *start,
                               size_t len) {
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!start) {
    return;
  }
  while (len > 0 && isspace((unsigned char)*start)) {
    start++;
    len--;
  }
  while (len > 0 && isspace((unsigned char)start[len - 1])) {
    len--;
  }
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, start, len);
  out[len] = '\0';
}

static void copy_parenthesized_source(char *out, size_t out_size, const char *source_start) {
  const char *end;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!source_start) {
    return;
  }
  source_start += 2;
  end = strchr(source_start, ')');
  if (!end) {
    return;
  }
  copy_trimmed_range(out, out_size, source_start, (size_t)(end - source_start));
}

static void performance_format_cpu_label(char *out, size_t out_size,
                                         const char *policy, long freq_khz) {
  if (!out || out_size == 0) {
    return;
  }
  if (!policy || !policy[0]) {
    copy_string(out, out_size, "launcher default");
  } else if (strcmp(policy, "fixed") == 0 && freq_khz > 0) {
    if (freq_khz % 1000 == 0) {
      snprintf(out, out_size, "%ld MHz", freq_khz / 1000);
    } else {
      snprintf(out, out_size, "%ld kHz", freq_khz);
    }
  } else {
    copy_string(out, out_size, policy);
  }
}

static int performance_cpu_preset_index(const char *policy, long freq_khz) {
  size_t i;

  if (!policy || !policy[0]) {
    return 0;
  }
  for (i = 0; i < PERFORMANCE_CPU_PRESET_COUNT; i++) {
    const struct performance_cpu_preset *preset = &PERFORMANCE_CPU_PRESETS[i];
    if (strcmp(policy, preset->policy) != 0) {
      continue;
    }
    if (strcmp(policy, "fixed") == 0 && freq_khz != preset->freq_khz) {
      continue;
    }
    return (int)i;
  }
  return 0;
}

static void performance_parse_current_cpu(struct ui_state *ui, const char *line) {
  const char *value;
  const char *source;
  char label[96];
  char *endptr = NULL;
  long freq;

  if (!ui || !line || strncmp(line, "current_cpu: ", 13) != 0) {
    return;
  }
  value = line + 13;
  source = strstr(value, " (");
  copy_trimmed_range(label, sizeof(label), value,
                     source ? (size_t)(source - value) : strlen(value));

  ui->performance_cpu_policy[0] = '\0';
  ui->performance_cpu_freq_khz = 0;
  if (strncmp(label, "fixed ", 6) == 0) {
    errno = 0;
    freq = strtol(label + 6, &endptr, 10);
    if (errno == 0 && endptr && freq > 0) {
      copy_string(ui->performance_cpu_policy, sizeof(ui->performance_cpu_policy), "fixed");
      ui->performance_cpu_freq_khz = freq;
    }
  } else if (strcmp(label, "performance") == 0) {
    copy_string(ui->performance_cpu_policy, sizeof(ui->performance_cpu_policy), label);
  }
  performance_format_cpu_label(ui->performance_cpu_label,
                               sizeof(ui->performance_cpu_label),
                               ui->performance_cpu_policy,
                               ui->performance_cpu_freq_khz);
}

static void performance_parse_current_cpu_cores(struct ui_state *ui, const char *line) {
  const char *value;
  const char *source;
  char label[96];
  char *endptr = NULL;
  long cores;

  if (!ui || !line || strncmp(line, "current_cpu_cores: ", 19) != 0) {
    return;
  }
  value = line + 19;
  source = strstr(value, " (");
  copy_trimmed_range(label, sizeof(label), value,
                     source ? (size_t)(source - value) : strlen(value));
  errno = 0;
  cores = strtol(label, &endptr, 10);
  if (errno == 0 && endptr && endptr != label &&
      (*endptr == '\0' || isspace((unsigned char)*endptr))) {
    ui->performance_cpu_cores = cores;
  }
}

static int load_performance_core_state(struct ui_state *ui) {
  char text_ui[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  char line[512];
  FILE *pipe;
  size_t pos = 0;
  int rc;

  if (!ui) {
    return 0;
  }
  ui->performance_cpu_policy[0] = '\0';
  ui->performance_cpu_freq_khz = 0;
  ui->performance_cpu_cores = 0;
  copy_string(ui->performance_cpu_label, sizeof(ui->performance_cpu_label),
              "launcher default");

  if (!performance_ensure_system(ui)) {
    return 0;
  }
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui")) {
    return 0;
  }
  if (!file_exists(text_ui)) {
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, " core system ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->performance_system_id) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    return 0;
  }

  pipe = popen(cmd, "r");
  if (!pipe) {
    return 0;
  }
  while (fgets(line, sizeof(line), pipe)) {
    performance_parse_current_cpu(ui, line);
    performance_parse_current_cpu_cores(ui, line);
  }
  rc = pclose(pipe);
  return rc == 0;
}

static void add_performance_settings_entries(struct ui_state *ui) {
  char value[256];

  load_performance_core_state(ui);
  add_setting_entry(ui, "performance_system", "System",
                    ui->performance_system_name[0] ? ui->performance_system_name : "-");
  add_setting_entry(ui, "performance_cpu_policy", "CPU freq",
                    ui->performance_cpu_label);
  if (ui->performance_cpu_cores > 0) {
    snprintf(value, sizeof(value), "%ld", ui->performance_cpu_cores);
  } else {
    copy_string(value, sizeof(value), "launcher default");
  }
  add_setting_entry(ui, "performance_cpu_cores", "CPU Cores", value);
  add_setting_entry(ui, "performance_clear_cpu_override", "Reset to Default", "");
}

static int load_settings_entries(struct ui_state *ui) {
  struct frontend_settings settings;

  ui->setting_count = 0;
  ui->settings_cursor = 0;
  if (!load_settings(ui->settings_path, &settings)) {
    return 0;
  }
  ui->frontend_settings = settings;
  load_theme_state(ui, settings.graphic_theme_id);
  apply_theme_setting_overrides(&ui->theme, &settings);
  switch (ui->settings_category) {
  case SETTINGS_CATEGORY_SYSTEM_DISPLAY_COLOR:
    load_device_settings(ui);
    add_system_display_color_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST:
    load_device_settings(ui);
    add_system_brightness_test_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM_TIME:
    load_device_settings(ui);
    add_system_time_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM_TIME_MANUAL:
    load_device_settings(ui);
    add_system_time_manual_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM_INFORMATION:
    load_device_settings(ui);
    add_system_information_entries(ui);
    break;
  case SETTINGS_CATEGORY_SYSTEM:
    load_device_settings(ui);
    add_system_settings_entries(ui);
    break;
  case SETTINGS_CATEGORY_NETWORK:
    load_device_settings(ui);
    add_network_settings_entries(ui);
    break;
  case SETTINGS_CATEGORY_NETWORK_SERVICE:
    load_device_settings(ui);
    add_network_service_entries(ui);
    break;
  case SETTINGS_CATEGORY_NETWORK_INFORMATION:
    load_device_settings(ui);
    add_network_information_entries(ui);
    break;
  case SETTINGS_CATEGORY_PERFORMANCE:
    load_device_settings(ui);
    add_performance_settings_entries(ui);
    break;
  case SETTINGS_CATEGORY_UI_THEME:
    add_ui_theme_settings_entries(ui, &settings);
    break;
  case SETTINGS_CATEGORY_UI:
  default:
    add_ui_settings_entries(ui, &settings);
    break;
  }
  return 1;
}

static void select_setting_entry_by_id(struct ui_state *ui, const char *id) {
  size_t i;

  if (!ui || !id) {
    return;
  }
  for (i = 0; i < ui->setting_count; i++) {
    if (strcmp(ui->setting_entries[i].id, id) == 0) {
      ui->settings_cursor = i;
      return;
    }
  }
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
      !run_scanner(ui->plumos_root, ui->sdcard_root, NULL, 0)) {
    copy_string(ui->status, sizeof(ui->status), "full scan failed or scanner is missing");
  }
  if (!load_settings(ui->settings_path, &settings)) {
    copy_string(ui->status, sizeof(ui->status), "settings read failed");
  }
  ui->frontend_settings = settings;

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
  if (settings.show_recent_on_top && ui->top_count < UI_MAX_TOP) {
    struct top_entry recent;
    memset(&recent, 0, sizeof(recent));
    copy_string(recent.id, sizeof(recent.id), "recent");
    copy_string(recent.display_name, sizeof(recent.display_name), "Recent");
    copy_string(recent.default_launch_profile, sizeof(recent.default_launch_profile),
                "internal:recent");
    recent.rom_count = count_json_array_objects(ui->recent_path, "recents");
    recent.pinned = 1;
    recent.virtual_entry = 1;
    ui->top_entries[ui->top_count++] = recent;
  }
  if (strcmp(settings.sort_systems, "name") == 0 && ui->top_count > 1) {
    qsort(ui->top_entries, ui->top_count, sizeof(ui->top_entries[0]), cmp_top_entry_name);
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
  copy_string(ui->menu_id, sizeof(ui->menu_id), "start");
  copy_string(ui->menu_title, sizeof(ui->menu_title), "START");
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

static int load_apps_menu_entries(struct ui_state *ui) {
  char *json;
  size_t json_size;
  const char *apps_start;
  const char *apps_end;
  const char *app_cursor;

  ui->menu_count = 0;
  ui->menu_cursor = 0;
  copy_string(ui->menu_id, sizeof(ui->menu_id), "apps");
  copy_string(ui->menu_title, sizeof(ui->menu_title), "Apps");
  json = read_file(ui->apps_path, &json_size);
  if (!json) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "apps", &apps_start, &apps_end)) {
    free(json);
    return 0;
  }

  app_cursor = apps_start;
  while (ui->menu_count < UI_MAX_MENU) {
    const char *obj_start;
    const char *obj_end;
    struct menu_entry entry;
    char menu_id[64] = "";
    int visible;

    if (!json_next_object(&app_cursor, apps_end, &obj_start, &obj_end)) {
      break;
    }
    memset(&entry, 0, sizeof(entry));
    visible = json_get_bool(obj_start, obj_end, "visible", 1);
    json_get_string(obj_start, obj_end, "id", entry.id, sizeof(entry.id));
    json_get_string(obj_start, obj_end, "display_name", entry.display_name,
                    sizeof(entry.display_name));
    json_get_string(obj_start, obj_end, "kind", entry.kind, sizeof(entry.kind));
    json_get_string(obj_start, obj_end, "launch_profile", entry.action,
                    sizeof(entry.action));
    json_get_string(obj_start, obj_end, "menu", menu_id, sizeof(menu_id));
    entry.confirm = json_get_bool(obj_start, obj_end, "confirm", 0);
    entry.background = json_get_bool(obj_start, obj_end, "background", 0);
    entry.show_results = json_get_bool(obj_start, obj_end, "show_results", 0);
    if (!visible || !entry.id[0] || strcmp(menu_id, "apps") != 0) {
      continue;
    }
    if (!entry.display_name[0]) {
      copy_string(entry.display_name, sizeof(entry.display_name), entry.id);
    }
    ui->menu_entries[ui->menu_count++] = entry;
  }
  free(json);
  return 1;
}

static void select_menu_entry_by_id(struct ui_state *ui, const char *id) {
  size_t i;

  if (!ui || !id) {
    return;
  }
  for (i = 0; i < ui->menu_count; i++) {
    if (strcmp(ui->menu_entries[i].id, id) == 0) {
      ui->menu_cursor = i;
      return;
    }
  }
}

static int system_scraper_enabled_from_json(const char *json, size_t json_size,
                                            const char *system_id) {
  const char *systems_start;
  const char *systems_end;
  const char *cursor;

  if (!json || !system_id || !system_id[0]) {
    return 0;
  }
  if (!json_find_array(json, json + json_size, "systems", &systems_start, &systems_end)) {
    return 0;
  }
  cursor = systems_start;
  while (1) {
    const char *obj_start;
    const char *obj_end;
    const char *scraper_start;
    const char *scraper_end;
    char id[64] = "";

    if (!json_next_object(&cursor, systems_end, &obj_start, &obj_end)) {
      break;
    }
    json_get_string(obj_start, obj_end, "id", id, sizeof(id));
    if (strcmp(id, system_id) != 0) {
      continue;
    }
    if (!json_find_object(obj_start, obj_end, "scraper", &scraper_start, &scraper_end)) {
      return 0;
    }
    return json_get_bool(scraper_start, scraper_end, "enabled", 0);
  }
  return 0;
}

static void clamp_scraping_choice_cursor(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  if (ui->scraping_choice_count == 0) {
    ui->scraping_choice_cursor = 0;
  } else if (ui->scraping_choice_cursor > ui->scraping_choice_count) {
    ui->scraping_choice_cursor = ui->scraping_choice_count;
  }
}

static void clamp_scraping_menu_cursor(struct ui_state *ui) {
  if (!ui || ui->scraping_menu_cursor >= UI_SCRAPING_FIELD_COUNT) {
    if (ui) {
      ui->scraping_menu_cursor = 0;
    }
  }
}

static void clamp_scraping_kind_cursor(struct ui_state *ui) {
  if (!ui ||
      ui->scraping_kind_cursor >=
          sizeof(SCRAPING_KIND_CHOICES) / sizeof(SCRAPING_KIND_CHOICES[0])) {
    if (ui) {
      ui->scraping_kind_cursor = 0;
    }
  }
}

static const struct scraping_kind_choice *scraping_selected_kind(struct ui_state *ui) {
  clamp_scraping_kind_cursor(ui);
  return &SCRAPING_KIND_CHOICES[ui ? ui->scraping_kind_cursor : 0];
}

static const char *scraping_existing_label(const struct ui_state *ui) {
  return ui && ui->scraping_replace_existing ? "Replace" : "Skip";
}

static void cycle_scraping_kind(struct ui_state *ui, int delta) {
  size_t count = sizeof(SCRAPING_KIND_CHOICES) / sizeof(SCRAPING_KIND_CHOICES[0]);

  if (!ui || count == 0) {
    return;
  }
  clamp_scraping_kind_cursor(ui);
  if (delta < 0) {
    ui->scraping_kind_cursor =
        ui->scraping_kind_cursor == 0 ? count - 1 : ui->scraping_kind_cursor - 1;
  } else {
    ui->scraping_kind_cursor =
        ui->scraping_kind_cursor + 1 >= count ? 0 : ui->scraping_kind_cursor + 1;
  }
}

static int load_scraping_choices(struct ui_state *ui) {
  char *json;
  size_t json_size;
  char selected_id[64] = "";
  size_t i;

  if (!ui) {
    return 0;
  }
  if (ui->scraping_choice_cursor > 0 &&
      ui->scraping_choice_cursor <= ui->scraping_choice_count) {
    copy_string(selected_id, sizeof(selected_id),
                ui->scraping_choices[ui->scraping_choice_cursor - 1].id);
  }
  ui->scraping_choice_count = 0;
  json = read_file(ui->systems_path, &json_size);
  if (!json) {
    ui->scraping_choice_cursor = 0;
    return 0;
  }
  for (i = 0; i < ui->top_count && ui->scraping_choice_count < UI_MAX_SCRAPING_CHOICES; i++) {
    const struct top_entry *top = &ui->top_entries[i];
    struct scraping_choice *choice;

    if (top->virtual_entry || top->rom_count <= 0 || !valid_system_id(top->id) ||
        !system_scraper_enabled_from_json(json, json_size, top->id)) {
      continue;
    }
    choice = &ui->scraping_choices[ui->scraping_choice_count++];
    memset(choice, 0, sizeof(*choice));
    copy_string(choice->id, sizeof(choice->id), top->id);
    copy_string(choice->display_name, sizeof(choice->display_name),
                top->display_name[0] ? top->display_name : top->id);
    choice->rom_count = top->rom_count;
  }
  free(json);
  ui->scraping_choice_cursor = 0;
  if (selected_id[0]) {
    for (i = 0; i < ui->scraping_choice_count; i++) {
      if (strcmp(ui->scraping_choices[i].id, selected_id) == 0) {
        ui->scraping_choice_cursor = i + 1;
        break;
      }
    }
  }
  clamp_scraping_choice_cursor(ui);
  return 1;
}

static long scraping_selected_rom_count(const struct ui_state *ui) {
  long total = 0;
  size_t i;

  if (!ui || ui->scraping_choice_count == 0) {
    return 0;
  }
  if (ui->scraping_choice_cursor > 0 &&
      ui->scraping_choice_cursor <= ui->scraping_choice_count) {
    return ui->scraping_choices[ui->scraping_choice_cursor - 1].rom_count;
  }
  for (i = 0; i < ui->scraping_choice_count; i++) {
    total += ui->scraping_choices[i].rom_count;
  }
  return total;
}

static const char *scraping_selected_label(const struct ui_state *ui) {
  if (!ui || ui->scraping_choice_count == 0) {
    return "NONE";
  }
  if (ui->scraping_choice_cursor > 0 &&
      ui->scraping_choice_cursor <= ui->scraping_choice_count) {
    return ui->scraping_choices[ui->scraping_choice_cursor - 1].display_name;
  }
  return "ALL";
}

static void add_thumbnail_result_line(struct ui_state *ui, const char *line) {
  if (!ui || !line || !line[0]) {
    return;
  }
  if (ui->thumbnail_result_count >= UI_THUMBNAIL_RESULT_MAX_LINES) {
    memmove(ui->thumbnail_result_lines, ui->thumbnail_result_lines + 1,
            sizeof(ui->thumbnail_result_lines[0]) *
                (UI_THUMBNAIL_RESULT_MAX_LINES - 1));
    ui->thumbnail_result_count = UI_THUMBNAIL_RESULT_MAX_LINES - 1;
  }
  copy_truncated_string(ui->thumbnail_result_lines[ui->thumbnail_result_count],
                        sizeof(ui->thumbnail_result_lines[ui->thumbnail_result_count]), line);
  ui->thumbnail_result_count++;
}

static const char *thumbnail_action_label(const char *id) {
  if (!id) {
    return "Thumbnail Task";
  }
  if (strcmp(id, "thumbnail-plan") == 0) {
    return "Thumbnail Plan";
  }
  if (strcmp(id, "thumbnail-fetch") == 0) {
    return "Fetch Thumbnails";
  }
  if (strcmp(id, "thumbnail-scraping") == 0) {
    return "Scraping";
  }
  return id;
}

static void clamp_thumbnail_result_cursor(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  if (ui->thumbnail_result_count == 0) {
    ui->thumbnail_result_cursor = 0;
  } else if (ui->thumbnail_result_cursor >= ui->thumbnail_result_count) {
    ui->thumbnail_result_cursor = ui->thumbnail_result_count - 1;
  }
}

static void reset_thumbnail_running_progress(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  ui->thumbnail_running_phase[0] = '\0';
  ui->thumbnail_running_system[0] = '\0';
  ui->thumbnail_progress_current = 0;
  ui->thumbnail_progress_total = 0;
  ui->thumbnail_progress_downloaded = 0;
  ui->thumbnail_progress_no_match = 0;
  ui->thumbnail_progress_failed = 0;
}

static int update_thumbnail_running_progress_from_log_line(struct ui_state *ui,
                                                           const char *line) {
  char buf[UI_RENDER_LINE_MAX];
  char *fields[10];
  char *save = NULL;
  char *token;
  size_t count = 0;
  long current = 0;
  long total = 0;
  long downloaded = 0;
  long no_match = 0;
  long failed = 0;

  if (!ui || !line || strncmp(line, "progress\t", 9) != 0) {
    return 0;
  }
  copy_truncated_string(buf, sizeof(buf), line);
  for (token = strtok_r(buf, "\t", &save);
       token && count < sizeof(fields) / sizeof(fields[0]);
       token = strtok_r(NULL, "\t", &save)) {
    fields[count++] = token;
  }
  if (count < 5 ||
      !parse_nonnegative_long(fields[3], &current) ||
      !parse_nonnegative_long(fields[4], &total)) {
    return 0;
  }
  if (count >= 6) {
    parse_nonnegative_long(fields[5], &downloaded);
  }
  if (count >= 7) {
    parse_nonnegative_long(fields[6], &no_match);
  }
  if (count >= 8) {
    parse_nonnegative_long(fields[7], &failed);
  }

  copy_string(ui->thumbnail_running_phase, sizeof(ui->thumbnail_running_phase),
              fields[1]);
  copy_string(ui->thumbnail_running_system, sizeof(ui->thumbnail_running_system),
              fields[2]);
  ui->thumbnail_progress_current = current;
  ui->thumbnail_progress_total = total;
  ui->thumbnail_progress_downloaded = downloaded;
  ui->thumbnail_progress_no_match = no_match;
  ui->thumbnail_progress_failed = failed;
  return 1;
}

static void add_thumbnail_result_from_log_line(struct ui_state *ui, const char *line) {
  char buf[UI_RENDER_LINE_MAX];
  char *fields[20];
  char *save = NULL;
  char *token;
  size_t count = 0;
  char out[UI_RENDER_LINE_MAX];

  if (!ui || !line || !line[0] || strncmp(line, "status\t", 7) == 0 ||
      strncmp(line, "progress\t", 9) == 0) {
    return;
  }
  copy_truncated_string(buf, sizeof(buf), line);
  for (token = strtok_r(buf, "\t", &save);
       token && count < sizeof(fields) / sizeof(fields[0]);
       token = strtok_r(NULL, "\t", &save)) {
    fields[count++] = token;
  }
  if (count == 0) {
    return;
  }
  if (strcmp(fields[0], "scraping_options") == 0 && count >= 4) {
    snprintf(out, sizeof(out), "%s", "options");
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "%s", fields[1]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "%s", fields[2]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "%s", fields[3]);
    add_thumbnail_result_line(ui, out);
    add_thumbnail_result_line(ui, "----------------");
  } else if (strcmp(fields[0], "plan") == 0 && count >= 8) {
    snprintf(out, sizeof(out), "%s", fields[1]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "reason %s", fields[3]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "aliases seen %s", fields[4]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "ROMs %s", fields[5]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "existing %s", fields[6]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "missing %s", fields[7]);
    add_thumbnail_result_line(ui, out);
    if (count >= 10) {
      snprintf(out, sizeof(out), "CRC workers %s", fields[8]);
      add_thumbnail_result_line(ui, out);
      snprintf(out, sizeof(out), "DL workers %s", fields[9]);
      add_thumbnail_result_line(ui, out);
    }
    add_thumbnail_result_line(ui, "----------------");
  } else if (strcmp(fields[0], "fetch") == 0 && count >= 15) {
    snprintf(out, sizeof(out), "%s", fields[1]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "reason %s", fields[3]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "aliases seen %s", fields[4]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "ROMs %s", fields[5]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "existing %s", fields[6]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "missing %s", fields[7]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "CRC checked %s", fields[8]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "CRC matched %s", fields[9]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "downloaded %s", fields[10]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "no match %s", fields[11]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "CRC miss %s", fields[12]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "image miss %s", fields[13]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "download failed %s", fields[14]);
    add_thumbnail_result_line(ui, out);
    if (count >= 18) {
      snprintf(out, sizeof(out), "invalid PNG %s", fields[15]);
      add_thumbnail_result_line(ui, out);
      snprintf(out, sizeof(out), "skipped zip %s", fields[16]);
      add_thumbnail_result_line(ui, out);
      snprintf(out, sizeof(out), "skipped tool %s", fields[17]);
      add_thumbnail_result_line(ui, out);
    }
    add_thumbnail_result_line(ui, "----------------");
  } else if (strcmp(fields[0], "disabled") == 0 && count >= 4) {
    snprintf(out, sizeof(out), "%s", fields[1]);
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "disabled %s", fields[3]);
    add_thumbnail_result_line(ui, out);
    add_thumbnail_result_line(ui, "----------------");
  } else if (strcmp(fields[0], "app_start") == 0 && count >= 2) {
    snprintf(out, sizeof(out), "%s", thumbnail_action_label(fields[1]));
    add_thumbnail_result_line(ui, out);
    add_thumbnail_result_line(ui, "started");
  } else if (strcmp(fields[0], "app_finish") == 0 && count >= 3) {
    snprintf(out, sizeof(out), "%s", thumbnail_action_label(fields[1]));
    add_thumbnail_result_line(ui, out);
    snprintf(out, sizeof(out), "finished %s", fields[2]);
    add_thumbnail_result_line(ui, out);
  } else if (strcmp(fields[0], "app_already_running") == 0 && count >= 2) {
    snprintf(out, sizeof(out), "%s is already running",
             thumbnail_action_label(fields[1]));
    add_thumbnail_result_line(ui, out);
  }
}

static int load_thumbnail_results(struct ui_state *ui) {
  char log_path[PATH_MAX];
  FILE *f;
  char line[UI_RENDER_LINE_MAX];

  if (!ui) {
    return 0;
  }
  ui->thumbnail_result_count = 0;
  if (!join_path(log_path, sizeof(log_path), ui->plumos_root,
                 "logs/frontend-apps-latest.log")) {
    add_thumbnail_result_line(ui, "result log path is too long");
    clamp_thumbnail_result_cursor(ui);
    return 0;
  }
  f = fopen(log_path, "rb");
  if (!f) {
    add_thumbnail_result_line(ui, "No thumbnail result yet");
    clamp_thumbnail_result_cursor(ui);
    return 1;
  }
  while (fgets(line, sizeof(line), f)) {
    trim_line_end(line);
    add_thumbnail_result_from_log_line(ui, line);
  }
  fclose(f);
  if (ui->thumbnail_result_count == 0) {
    add_thumbnail_result_line(ui, "No thumbnail result yet");
  }
  clamp_thumbnail_result_cursor(ui);
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
    const char *media_start;
    const char *media_end;
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
    if (json_find_object(obj_start, obj_end, "media", &media_start, &media_end)) {
      json_get_string(media_start, media_end, "thumbnail", entry.thumbnail,
                      sizeof(entry.thumbnail));
    }
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
    const char *media_start;
    const char *media_end;
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
    if (json_find_object(obj_start, obj_end, "media", &media_start, &media_end)) {
      json_get_string(media_start, media_end, "thumbnail", entry.thumbnail,
                      sizeof(entry.thumbnail));
    }
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
  struct frontend_settings settings;
  int cache_exists;
  int scan_on_enter;
  int with_thumbnails;

  ui->rom_count = 0;
  ui->rom_cursor = 0;
  ui->rom_scan_background_started = 0;
  if (strcmp(system_id, "favorites") == 0) {
    return load_favorite_entries(ui);
  }
  if (!build_system_cache_path(path, sizeof(path), ui->plumos_root, system_id)) {
    return 0;
  }
  if (!load_settings(ui->settings_path, &settings)) {
    copy_string(ui->status, sizeof(ui->status), "settings read failed; using scan defaults");
  }
  ui->frontend_settings = settings;
  with_thumbnails =
      strcmp(settings.rom_mode[0] ? settings.rom_mode : settings.ui_mode, "graphic") == 0;
  cache_exists = file_exists(path);
  scan_on_enter = rom_scan_policy_is_on_enter(settings.rom_scan_policy);
  if (!cache_exists) {
    if (!run_scanner(ui->plumos_root, ui->sdcard_root, system_id, with_thumbnails)) {
      return 0;
    }
    cache_exists = file_exists(path);
    if (!cache_exists) {
      return 0;
    }
  } else if ((scan_on_enter || ui->refresh) && !ui->rom_scan_refresh_suppressed) {
    if (with_thumbnails) {
      if (!run_scanner(ui->plumos_root, ui->sdcard_root, system_id, 1)) {
        copy_string(ui->status, sizeof(ui->status),
                    "thumbnail scan failed; using cached ROM list");
      }
    } else {
      trigger_rom_scan_refresh(ui, system_id, 0);
    }
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
    const char *media_start;
    const char *media_end;
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
    if (json_find_object(obj_start, obj_end, "media", &media_start, &media_end)) {
      json_get_string(media_start, media_end, "thumbnail", entry.thumbnail,
                      sizeof(entry.thumbnail));
    }
    if (!entry.title[0]) {
      copy_string(entry.title, sizeof(entry.title), entry.relative_path);
    }
    ui->rom_entries[ui->rom_count++] = entry;
  }
  free(json);
  if (ui->rom_count > 1) {
    if (strcmp(settings.sort_roms, "path") == 0) {
      qsort(ui->rom_entries, ui->rom_count, sizeof(ui->rom_entries[0]), cmp_rom_entry_path);
    } else {
      qsort(ui->rom_entries, ui->rom_count, sizeof(ui->rom_entries[0]), cmp_rom_entry_name);
    }
  }
  return 1;
}

static void ui_append_render_line(struct ui_state *ui, const char *line, size_t len) {
  if (ui->render_line_count >= UI_RENDER_MAX_LINES) {
    return;
  }
  if (len >= UI_RENDER_LINE_MAX) {
    len = UI_RENDER_LINE_MAX - 1;
  }
  memcpy(ui->render_lines[ui->render_line_count], line, len);
  ui->render_lines[ui->render_line_count][len] = '\0';
  ui->render_line_count++;
}

static void core_append_line(struct ui_state *ui, const char *line) {
  size_t len;
  if (ui->core_line_count >= UI_RENDER_MAX_LINES || !line) {
    return;
  }
  len = strlen(line);
  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
    len--;
  }
  if (len >= UI_RENDER_LINE_MAX) {
    len = UI_RENDER_LINE_MAX - 1;
  }
  memcpy(ui->core_lines[ui->core_line_count], line, len);
  ui->core_lines[ui->core_line_count][len] = '\0';
  ui->core_line_count++;
}

static const char *core_profile_display_name(const char *profile) {
  if (!profile || !profile[0]) {
    return "auto";
  }
  return profile;
}

static int core_menu_row_is_selectable(size_t row) {
  return row < CORE_MENU_ROW_COUNT && row != CORE_MENU_ROW_SEPARATOR;
}

static void core_menu_clamp_cursor(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  if (ui->core_menu_cursor >= CORE_MENU_ROW_COUNT) {
    ui->core_menu_cursor = CORE_MENU_ROW_PROFILE;
  }
  if (!core_menu_row_is_selectable(ui->core_menu_cursor)) {
    ui->core_menu_cursor = CORE_MENU_ROW_CPU_FREQ;
  }
}

static void move_core_menu_cursor(struct ui_state *ui, int direction) {
  size_t row;

  if (!ui || direction == 0) {
    return;
  }
  core_menu_clamp_cursor(ui);
  row = ui->core_menu_cursor;
  while (1) {
    if (direction > 0) {
      row = (row + 1) % CORE_MENU_ROW_COUNT;
    } else if (row == 0) {
      row = CORE_MENU_ROW_COUNT - 1;
    } else {
      row--;
    }
    if (core_menu_row_is_selectable(row)) {
      ui->core_menu_cursor = row;
      return;
    }
  }
}

static void reset_core_profile_choices(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  memset(ui->core_profiles, 0, sizeof(ui->core_profiles));
  ui->core_profile_count = 0;
  ui->core_profile_cursor = 0;
  ui->core_current_profile[0] = '\0';
  ui->core_current_source[0] = '\0';
  ui->core_cpu_policy[0] = '\0';
  ui->core_cpu_freq_khz = 0;
  ui->core_cpu_cores = 0;
  copy_string(ui->core_cpu_label, sizeof(ui->core_cpu_label), "launcher default");
  copy_string(ui->core_cpu_source, sizeof(ui->core_cpu_source), "unavailable");
  copy_string(ui->core_cpu_cores_source, sizeof(ui->core_cpu_cores_source),
              "unavailable");
  ui->core_line_count = 0;
}

static int add_core_profile_choice(struct ui_state *ui, const char *profile) {
  size_t i;

  if (!ui || !valid_launch_profile_id(profile)) {
    return 0;
  }
  for (i = 0; i < ui->core_profile_count; i++) {
    if (strcmp(ui->core_profiles[i].id, profile) == 0) {
      return 1;
    }
  }
  if (ui->core_profile_count >= UI_MAX_CORE_PROFILES) {
    return 0;
  }
  if (!copy_string(ui->core_profiles[ui->core_profile_count].id,
                   sizeof(ui->core_profiles[ui->core_profile_count].id), profile)) {
    return 0;
  }
  ui->core_profile_count++;
  return 1;
}

static void parse_core_current_profile_line(struct ui_state *ui, const char *line) {
  const char *prefix = "current_profile:";
  const char *value;
  const char *source_start;
  const char *source_end;

  if (!ui || !line || strncmp(line, prefix, strlen(prefix)) != 0) {
    return;
  }
  value = line + strlen(prefix);
  while (*value == ' ') {
    value++;
  }
  source_start = strstr(value, " (");
  if (source_start) {
    copy_trimmed_range(ui->core_current_profile, sizeof(ui->core_current_profile),
                       value, (size_t)(source_start - value));
    source_start += 2;
    source_end = strchr(source_start, ')');
    if (source_end) {
      copy_trimmed_range(ui->core_current_source, sizeof(ui->core_current_source),
                         source_start, (size_t)(source_end - source_start));
    }
  } else {
    copy_truncated_string(ui->core_current_profile, sizeof(ui->core_current_profile), value);
  }
}

static void parse_core_current_cpu_line(struct ui_state *ui, const char *line) {
  const char *value;
  const char *source;
  char label[96];
  char *endptr = NULL;
  long freq;

  if (!ui || !line || strncmp(line, "current_cpu: ", 13) != 0) {
    return;
  }
  value = line + 13;
  source = strstr(value, " (");
  copy_trimmed_range(label, sizeof(label), value,
                     source ? (size_t)(source - value) : strlen(value));
  if (source) {
    copy_parenthesized_source(ui->core_cpu_source, sizeof(ui->core_cpu_source),
                              source);
  }

  ui->core_cpu_policy[0] = '\0';
  ui->core_cpu_freq_khz = 0;
  if (strncmp(label, "fixed ", 6) == 0) {
    errno = 0;
    freq = strtol(label + 6, &endptr, 10);
    if (errno == 0 && endptr && freq > 0) {
      copy_string(ui->core_cpu_policy, sizeof(ui->core_cpu_policy), "fixed");
      ui->core_cpu_freq_khz = freq;
    }
  } else if (strcmp(label, "performance") == 0) {
    copy_string(ui->core_cpu_policy, sizeof(ui->core_cpu_policy), label);
  }
  performance_format_cpu_label(ui->core_cpu_label, sizeof(ui->core_cpu_label),
                               ui->core_cpu_policy, ui->core_cpu_freq_khz);
}

static void parse_core_current_cpu_cores_line(struct ui_state *ui,
                                              const char *line) {
  const char *value;
  const char *source;
  char label[96];
  char *endptr = NULL;
  long cores;

  if (!ui || !line || strncmp(line, "current_cpu_cores: ", 19) != 0) {
    return;
  }
  value = line + 19;
  source = strstr(value, " (");
  copy_trimmed_range(label, sizeof(label), value,
                     source ? (size_t)(source - value) : strlen(value));
  if (source) {
    copy_parenthesized_source(ui->core_cpu_cores_source,
                              sizeof(ui->core_cpu_cores_source), source);
  }
  errno = 0;
  cores = strtol(label, &endptr, 10);
  if (errno == 0 && endptr && endptr != label &&
      (*endptr == '\0' || isspace((unsigned char)*endptr))) {
    ui->core_cpu_cores = cores;
  }
}

static int parse_core_profile_choice_line(struct ui_state *ui, const char *line) {
  const char *p = line;
  char profile[128];
  size_t len = 0;

  if (!ui || !line) {
    return 0;
  }
  while (*p == ' ') {
    p++;
  }
  if (!isdigit((unsigned char)*p)) {
    return 0;
  }
  while (isdigit((unsigned char)*p)) {
    p++;
  }
  if (*p != '.') {
    return 0;
  }
  p++;
  while (*p == ' ') {
    p++;
  }
  while (*p && *p != ' ' && len + 1 < sizeof(profile)) {
    profile[len++] = *p++;
  }
  profile[len] = '\0';
  if (!valid_launch_profile_id(profile)) {
    return 0;
  }
  return add_core_profile_choice(ui, profile);
}

static void select_current_core_profile(struct ui_state *ui) {
  size_t i;

  if (!ui || !ui->core_current_profile[0]) {
    return;
  }
  for (i = 0; i < ui->core_profile_count; i++) {
    if (strcmp(ui->core_profiles[i].id, ui->core_current_profile) == 0) {
      ui->core_profile_cursor = i;
      return;
    }
  }
}

static void ui_vprintf(struct ui_state *ui, const char *fmt, va_list ap) {
  char buf[512];
  const char *start;
  const char *p;

  if (!ui->renderer_mali) {
    vprintf(fmt, ap);
    return;
  }

  vsnprintf(buf, sizeof(buf), fmt, ap);
  start = buf;
  for (p = buf; ; p++) {
    if (*p == '\n' || *p == '\0') {
      if (p > start || *p == '\n') {
        ui_append_render_line(ui, start, (size_t)(p - start));
      }
      if (*p == '\0') {
        break;
      }
      start = p + 1;
    }
  }
}

static void ui_printf(struct ui_state *ui, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  ui_vprintf(ui, fmt, ap);
  va_end(ap);
}

static void clear_screen(struct ui_state *ui) {
  if (ui->renderer_mali) {
    ui->render_line_count = 0;
    return;
  }
  if (!ui->no_clear) {
    printf("\033[2J\033[H");
  }
}

static int ui_uses_graphic_mode(const struct ui_state *ui) {
  return ui && ui->renderer_mali &&
         strcmp(ui->frontend_settings.ui_mode, "graphic") == 0;
}

#define UI_GRAPHIC_TOP_GRID_COLUMNS 3
#define UI_GRAPHIC_TOP_GRID_ROWS 2
#define UI_GRAPHIC_TOP_GRID_PAGE_SIZE \
  (UI_GRAPHIC_TOP_GRID_COLUMNS * UI_GRAPHIC_TOP_GRID_ROWS)
#define UI_GRAPHIC_TOP_STRIP_COLUMNS 2
#define UI_GRAPHIC_TOP_STRIP_ROWS 1
#define UI_GRAPHIC_TOP_STRIP_PAGE_SIZE \
  (UI_GRAPHIC_TOP_STRIP_COLUMNS * UI_GRAPHIC_TOP_STRIP_ROWS)
#define UI_GRAPHIC_ROM_PAGE_SIZE 10
#define UI_GRAPHIC_TOP_TRANSITION_DEFAULT_MS 260

static int ui_graphic_top_uses_strip(const struct ui_state *ui) {
  return ui_uses_graphic_mode(ui) &&
         strcmp(ui->theme.graphic_top_layout, "tile_strip") == 0;
}

static size_t ui_graphic_top_page_size(const struct ui_state *ui) {
  return ui_graphic_top_uses_strip(ui) ? UI_GRAPHIC_TOP_STRIP_PAGE_SIZE
                                       : UI_GRAPHIC_TOP_GRID_PAGE_SIZE;
}

static size_t ui_graphic_top_columns(const struct ui_state *ui) {
  return ui_graphic_top_uses_strip(ui) ? UI_GRAPHIC_TOP_STRIP_COLUMNS
                                       : UI_GRAPHIC_TOP_GRID_COLUMNS;
}

static size_t ui_list_window_size(const struct ui_state *ui) {
  if (ui_uses_graphic_mode(ui)) {
    if (ui->screen == SCREEN_TOP) {
      return ui_graphic_top_page_size(ui);
    }
    if (ui->screen == SCREEN_ROMS || ui->screen == SCREEN_FAVORITES ||
        ui->screen == SCREEN_RECENT) {
      return UI_GRAPHIC_ROM_PAGE_SIZE;
    }
  }
  if (ui && ui->renderer_mali) {
    if (strcmp(ui->mali_tty_entry_scale, "2") == 0 ||
        strcmp(ui->mali_tty_entry_scale, "2.0") == 0 ||
        strcmp(ui->mali_tty_entry_scale, "20") == 0) {
      return 8;
    }
    if (strcmp(ui->mali_tty_entry_scale, "1.5") == 0 ||
        strcmp(ui->mali_tty_entry_scale, "15") == 0) {
      return 10;
    }
    return 15;
  }
  return 10;
}

static size_t ui_graphic_top_page_for_cursor(const struct ui_state *ui,
                                             size_t cursor) {
  size_t page_size = ui_graphic_top_page_size(ui);
  if (page_size == 0) {
    page_size = 1;
  }
  return cursor / page_size;
}

static int ui_graphic_top_slide_enabled(const struct ui_state *ui) {
  return ui && ui_uses_graphic_mode(ui) && ui->screen == SCREEN_TOP &&
         strcmp(ui->theme.graphic_transition, "slide") == 0;
}

static long ui_graphic_top_transition_duration_ms(const struct ui_state *ui) {
  long duration;

  if (!ui_graphic_top_slide_enabled(ui)) {
    return 0;
  }
  duration = ui->theme.graphic_transition_ms > 0
                 ? ui->theme.graphic_transition_ms
                 : UI_GRAPHIC_TOP_TRANSITION_DEFAULT_MS;
  if (duration < 80) {
    duration = 80;
  } else if (duration > 1000) {
    duration = 1000;
  }
  return duration;
}

static void ui_start_graphic_top_transition(struct ui_state *ui,
                                            size_t from_cursor,
                                            size_t to_cursor) {
  size_t from_page;
  size_t to_page;
  long duration;

  if (!ui || from_cursor == to_cursor || !ui_graphic_top_slide_enabled(ui)) {
    return;
  }
  from_page = ui_graphic_top_page_for_cursor(ui, from_cursor);
  to_page = ui_graphic_top_page_for_cursor(ui, to_cursor);
  if (from_page == to_page) {
    return;
  }
  duration = ui_graphic_top_transition_duration_ms(ui);
  if (duration <= 0) {
    return;
  }
  ui->top_transition_from_cursor = from_cursor;
  ui->top_transition_from_page = from_page;
  ui->top_transition_to_page = to_page;
  ui->top_transition_start_ms = current_time_ms();
  ui->top_transition_duration_ms = duration;
  ui->top_transition_direction = to_cursor > from_cursor ? 1 : -1;
  ui->top_transition_active = 1;
}

static double ui_graphic_top_transition_progress(struct ui_state *ui) {
  long long elapsed;
  double progress;

  if (!ui || !ui->top_transition_active ||
      ui->top_transition_duration_ms <= 0) {
    return 1.0;
  }
  elapsed = current_time_ms() - ui->top_transition_start_ms;
  if (elapsed >= ui->top_transition_duration_ms) {
    ui->top_transition_active = 0;
    return 1.0;
  }
  if (elapsed <= 0) {
    return 0.0;
  }
  progress = (double)elapsed / (double)ui->top_transition_duration_ms;
  return progress;
}

static void ui_move_graphic_top_cursor(struct ui_state *ui, enum ui_action action) {
  size_t cursor;
  size_t next;

  if (!ui || ui->top_count == 0) {
    return;
  }
  cursor = ui->top_cursor;
  next = cursor;
  if (ui_graphic_top_uses_strip(ui)) {
    if (action == ACTION_LEFT && cursor > 0) {
      next = cursor - 1;
    } else if (action == ACTION_RIGHT && cursor + 1 < ui->top_count) {
      next = cursor + 1;
    }
  } else if (action == ACTION_LEFT) {
    if (cursor > 0) {
      next = cursor - 1;
    }
  } else if (action == ACTION_RIGHT) {
    if (cursor + 1 < ui->top_count) {
      next = cursor + 1;
    }
  } else if (action == ACTION_UP) {
    size_t columns = ui_graphic_top_columns(ui);
    if (cursor >= columns) {
      next = cursor - columns;
    }
  } else if (action == ACTION_DOWN) {
    size_t columns = ui_graphic_top_columns(ui);
    next = cursor + columns;
    if (next >= ui->top_count) {
      size_t next_row_start =
          ((cursor / columns) + 1) * columns;
      if (next_row_start < ui->top_count) {
        next = ui->top_count - 1;
      } else {
        next = cursor;
      }
    }
  }
  if (next != cursor) {
    ui_start_graphic_top_transition(ui, cursor, next);
    ui->top_cursor = next;
  }
}

static void reset_marquee(struct ui_state *ui);

static void ui_start_gallery_transition(struct ui_state *ui, size_t from_cursor,
                                        size_t to_cursor) {
  if (!ui || from_cursor == to_cursor || ui->rom_count == 0) {
    return;
  }
  ui->gallery_transition_from_cursor = from_cursor;
  ui->gallery_transition_to_cursor = to_cursor;
  ui->gallery_transition_start_ms = current_time_ms();
  ui->gallery_transition_duration_ms = UI_GALLERY_TRANSITION_MS;
  ui->gallery_transition_direction = to_cursor > from_cursor ? 1 : -1;
  ui->gallery_transition_active = 1;
}

static int ui_start_pending_gallery_transition(struct ui_state *ui) {
  size_t from_cursor;
  size_t to_cursor;

  if (!ui || !ui->gallery_pending_active || ui->rom_count == 0) {
    return 0;
  }
  from_cursor = ui->rom_cursor;
  to_cursor = ui->gallery_pending_cursor;
  ui->gallery_pending_active = 0;
  if (to_cursor >= ui->rom_count) {
    to_cursor = ui->rom_count - 1;
  }
  if (to_cursor == from_cursor) {
    return 0;
  }
  ui_start_gallery_transition(ui, from_cursor, to_cursor);
  ui->rom_cursor = to_cursor;
  reset_marquee(ui);
  return 1;
}

static double ui_gallery_transition_progress(struct ui_state *ui) {
  long long elapsed;
  double progress;

  if (!ui || !ui->gallery_transition_active ||
      ui->gallery_transition_duration_ms <= 0) {
    return 1.0;
  }
  elapsed = current_time_ms() - ui->gallery_transition_start_ms;
  if (elapsed >= ui->gallery_transition_duration_ms) {
    ui->gallery_transition_active = 0;
    return 1.0;
  }
  if (elapsed <= 0) {
    return 0.0;
  }
  progress = (double)elapsed / (double)ui->gallery_transition_duration_ms;
  return progress;
}

static void ui_emit_graphic_theme_color(struct ui_state *ui, const char *key,
                                        const char *value) {
  if (!ui || !key || !key[0] || !value || !value[0]) {
    return;
  }
  ui_printf(ui, "graphic_theme_color\t%s\t%s\n", key, value);
}

static void ui_emit_graphic_theme_motion(struct ui_state *ui, const char *key,
                                         const char *value) {
  if (!ui || !key || !key[0] || !value || !value[0]) {
    return;
  }
  ui_printf(ui, "graphic_theme_motion\t%s\t%s\n", key, value);
}

static void ui_emit_graphic_theme_asset(struct ui_state *ui, const char *key,
                                        const char *asset) {
  char resolved[PATH_MAX];

  if (!ui || !key || !key[0] || !asset || !asset[0]) {
    return;
  }
  if (!resolve_theme_asset_path(&ui->theme, asset, resolved, sizeof(resolved))) {
    return;
  }
  ui_printf(ui, "graphic_theme_asset\t%s\t%s\n", key, resolved);
}

static void ui_emit_graphic_theme_gallery_background(struct ui_state *ui) {
  static const char *candidates[] = {
      "images/rom_gallery_background.png",
      "images/rom-gallery-background.png",
      "images/gallery_background.png",
      "images/gallery-background.png",
      "images/games_background.png",
  };
  char resolved[PATH_MAX];
  size_t i;

  if (!ui) {
    return;
  }
  if (ui->theme.gallery_background[0]) {
    ui_emit_graphic_theme_asset(ui, "gallery_background",
                                ui->theme.gallery_background);
    return;
  }
  for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (resolve_theme_asset_path(&ui->theme, candidates[i], resolved,
                                 sizeof(resolved))) {
      ui_printf(ui, "graphic_theme_asset\tgallery_background\t%s\n",
                resolved);
      return;
    }
  }
}

static void ui_emit_graphic_theme(struct ui_state *ui) {
  if (!ui || !ui->theme.loaded || ui->theme.fallback) {
    return;
  }
  ui_printf(ui, "graphic_theme_id=%s\n", ui->theme.id[0] ? ui->theme.id : "default");
  ui_emit_graphic_theme_asset(ui, "background", ui->theme.background);
  ui_emit_graphic_theme_gallery_background(ui);
  ui_emit_graphic_theme_asset(ui, "placeholder", ui->theme.placeholder_thumbnail);
  ui_emit_graphic_theme_color(ui, "background", ui->theme.color_background);
  ui_emit_graphic_theme_color(ui, "foreground", ui->theme.color_foreground);
  ui_emit_graphic_theme_color(ui, "muted", ui->theme.color_muted);
  ui_emit_graphic_theme_color(ui, "accent", ui->theme.color_accent);
  ui_emit_graphic_theme_color(ui, "panel", ui->theme.color_panel);
  ui_emit_graphic_theme_color(ui, "panel_inner", ui->theme.color_panel_inner);
  ui_emit_graphic_theme_color(ui, "media_panel", ui->theme.color_media_panel);
  ui_emit_graphic_theme_color(ui, "selection_background",
                              ui->theme.color_selection_background);
  ui_emit_graphic_theme_color(ui, "selection_foreground",
                              ui->theme.color_selection_foreground);
  ui_emit_graphic_theme_color(ui, "danger", ui->theme.color_danger);
  ui_emit_graphic_theme_motion(ui, "top_layout", ui->theme.graphic_top_layout);
  ui_emit_graphic_theme_motion(ui, "transition", ui->theme.graphic_transition);
  ui_emit_graphic_theme_motion(ui, "transition_axis",
                               ui->theme.graphic_transition_axis);
  ui_emit_graphic_theme_motion(ui, "transition_easing",
                               ui->theme.graphic_transition_easing);
}

static void ui_emit_graphic_top_entry(struct ui_state *ui,
                                      const struct top_entry *entry,
                                      const char *prefix,
                                      int selected) {
  char logo_path[PATH_MAX] = "";

  if (!ui || !entry || !prefix) {
    return;
  }
  resolve_theme_system_logo_path(ui, entry->id, logo_path, sizeof(logo_path));
  ui_printf(ui, "%s\t%d\t%s\t\t%s\n", prefix, selected ? 1 : 0,
            entry->display_name, logo_path);
}

static void ui_emit_graphic_rom_entry(struct ui_state *ui,
                                      const struct rom_entry *entry,
                                      const char *prefix,
                                      int selected) {
  const char *detail;

  if (!ui || !entry || !prefix) {
    return;
  }
  detail = entry->detail[0] ? entry->detail : entry->relative_path;
  ui_printf(ui, "%s\t%d\t%s\t%s\t%s\n", prefix, selected ? 1 : 0,
            entry->title, detail, entry->thumbnail);
}

static void render_top_graphic(struct ui_state *ui, size_t start, size_t end) {
  size_t i;
  double transition_progress = 1.0;
  int transition_active = 0;

  ui_printf(ui, "plumOS controller UI - TOP\n");
  ui_printf(ui, "graphic_mode=top\n");
  ui_emit_graphic_theme(ui);
  ui_printf(ui, "graphic_entries=%zu cursor=%zu\n", ui->top_count,
            ui->top_count ? ui->top_cursor + 1 : 0);

  if (ui->top_transition_active &&
      ui->top_transition_to_page ==
          ui_graphic_top_page_for_cursor(ui, ui->top_cursor) &&
      ui_graphic_top_slide_enabled(ui)) {
    transition_progress = ui_graphic_top_transition_progress(ui);
    transition_active = ui->top_transition_active && transition_progress < 1.0;
  }
  if (transition_active) {
    size_t page_size = ui_graphic_top_page_size(ui);
    size_t prev_start = ui->top_transition_from_page * page_size;
    size_t prev_end = prev_start + page_size;

    if (prev_end > ui->top_count) {
      prev_end = ui->top_count;
    }
    ui_printf(ui, "graphic_transition=slide\n");
    ui_printf(ui, "graphic_transition_direction=%d\n",
              ui->top_transition_direction < 0 ? -1 : 1);
    ui_printf(ui, "graphic_transition_progress=%.3f\n", transition_progress);
    for (i = prev_start; i < prev_end; i++) {
      ui_emit_graphic_top_entry(ui, &ui->top_entries[i], "graphic_prev_entry",
                                i == ui->top_transition_from_cursor);
    }
  }

  for (i = start; i < end; i++) {
    const struct top_entry *entry = &ui->top_entries[i];
    ui_emit_graphic_top_entry(ui, entry, "graphic_entry",
                              i == ui->top_cursor);
  }
  if (ui->top_count == 0) {
    ui_printf(ui, "graphic_entry\t1\tNo Systems\t\t\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "status: %s\n", ui->status);
  }
}

static void ui_cursor_page_down(size_t *cursor, size_t count, size_t page_size) {
  if (!cursor || count == 0) {
    return;
  }
  if (page_size == 0) {
    page_size = 1;
  }
  if (*cursor + page_size >= count) {
    *cursor = count - 1;
  } else {
    *cursor += page_size;
  }
}

static void ui_cursor_page_up(size_t *cursor, size_t page_size) {
  if (!cursor) {
    return;
  }
  if (page_size == 0) {
    page_size = 1;
  }
  if (*cursor <= page_size) {
    *cursor = 0;
  } else {
    *cursor -= page_size;
  }
}

static void render_top(struct ui_state *ui) {
  size_t i;
  size_t window = ui_list_window_size(ui);
  size_t start = 0;
  size_t end;

  if (window == 0) {
    window = 1;
  }
  start = (ui->top_cursor / window) * window;
  end = start + window;
  if (end > ui->top_count) {
    end = ui->top_count;
  }

  if (ui_uses_graphic_mode(ui)) {
    render_top_graphic(ui, start, end);
    return;
  }

  ui_printf(ui, "plumOS controller UI - TOP\n");
  ui_printf(ui, "A: open  LEFT/RIGHT: page  START: menu  SELECT: core menu  FUNCTION: safe menu  Q: quit\n");
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->top_count, ui->top_count ? ui->top_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = start; i < end; i++) {
    const struct top_entry *entry = &ui->top_entries[i];
    ui_printf(ui, "%c %3zu  %s\n", i == ui->top_cursor ? '>' : ' ', i + 1,
              entry->display_name);
  }
  if (ui->top_count == 0) {
    ui_printf(ui, "(system entry is empty; run plumos-library-scan first)\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_roms_graphic(struct ui_state *ui, const char *title,
                                size_t start, size_t end) {
  size_t i;
  const char *mode = "roms";
  const char *base_title;
  const char *count_label = "ROMS";
  char counted_title[160];

  if (ui->screen == SCREEN_FAVORITES) {
    mode = "favorites";
    count_label = "ENTRIES";
  } else if (ui->screen == SCREEN_RECENT) {
    mode = "recent";
    count_label = "ENTRIES";
  }
  base_title = ui->current_system_name[0] ? ui->current_system_name : title;
  snprintf(counted_title, sizeof(counted_title), "%s  %s %zu", base_title,
           count_label, ui->rom_count);

  ui_printf(ui, "plumOS controller UI - %s\n", title);
  ui_printf(ui, "graphic_mode=%s\n", mode);
  ui_printf(ui, "graphic_system=%s\n", counted_title);
  ui_emit_graphic_theme(ui);
  ui_printf(ui, "graphic_entries=%zu cursor=%zu\n", ui->rom_count,
            ui->rom_count ? ui->rom_cursor + 1 : 0);
  for (i = start; i < end; i++) {
    const struct rom_entry *entry = &ui->rom_entries[i];
    ui_emit_graphic_rom_entry(ui, entry, "graphic_entry",
                              i == ui->rom_cursor);
  }
  if (ui->rom_count == 0) {
    ui_printf(ui, "graphic_entry\t1\tNo Entries\t-\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "status: %s\n", ui->status);
  }
}

static void ui_emit_gallery_window(struct ui_state *ui, const char *prefix,
                                   size_t cursor) {
  size_t start;
  size_t end;
  size_t i;

  if (!ui || !prefix || ui->rom_count == 0) {
    return;
  }
  if (cursor >= ui->rom_count) {
    cursor = ui->rom_count - 1;
  }
  start = cursor > 2 ? cursor - 2 : 0;
  end = cursor + 2;
  if (end >= ui->rom_count) {
    end = ui->rom_count - 1;
  }
  for (i = start; i <= end; i++) {
    ui_emit_graphic_rom_entry(ui, &ui->rom_entries[i], prefix,
                              i == cursor);
  }
}

static void render_gallery(struct ui_state *ui) {
  double transition_progress = 1.0;
  int transition_active = 0;
  const char *base_title;
  char counted_title[160];

  base_title = ui->current_system_name[0] ? ui->current_system_name : "ROMS";
  snprintf(counted_title, sizeof(counted_title), "%s  ROMS %zu", base_title,
           ui->rom_count);

  ui_printf(ui, "plumOS controller UI - Gallery\n");
  ui_printf(ui, "graphic_mode=gallery\n");
  ui_printf(ui, "graphic_system=%s\n", counted_title);
  ui_emit_graphic_theme(ui);

  if (ui->gallery_transition_active &&
      ui->gallery_transition_to_cursor == ui->rom_cursor) {
    transition_progress = ui_gallery_transition_progress(ui);
    if (!ui->gallery_transition_active && ui->gallery_pending_active &&
        ui_start_pending_gallery_transition(ui)) {
      transition_progress = ui_gallery_transition_progress(ui);
    }
  } else if (!ui->gallery_transition_active &&
             ui->gallery_pending_active &&
             ui_start_pending_gallery_transition(ui)) {
    transition_progress = ui_gallery_transition_progress(ui);
  }
  transition_active =
      ui->gallery_transition_active &&
      ui->gallery_transition_to_cursor == ui->rom_cursor &&
      transition_progress < 1.0;

  ui_printf(ui, "graphic_entries=%zu cursor=%zu\n", ui->rom_count,
            ui->rom_count ? ui->rom_cursor + 1 : 0);

  if (transition_active) {
    ui_printf(ui, "graphic_transition=slide\n");
    ui_printf(ui, "graphic_transition_direction=%d\n",
              ui->gallery_transition_direction < 0 ? -1 : 1);
    ui_printf(ui, "graphic_transition_progress=%.3f\n", transition_progress);
    ui_emit_gallery_window(ui, "graphic_prev_entry",
                           ui->gallery_transition_from_cursor);
  }
  ui_emit_gallery_window(ui, "graphic_entry", ui->rom_cursor);
  if (ui->rom_count == 0) {
    ui_printf(ui, "graphic_entry\t1\tNo Entries\t-\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "status: %s\n", ui->status);
  }
}

static void render_roms(struct ui_state *ui) {
  size_t i;
  size_t window = ui_list_window_size(ui);
  size_t start = 0;
  size_t end;
  const char *title = "ROMS";
  const char *subtitle =
      "A: launch  B: TOP  LEFT/RIGHT: page  START: menu  SELECT: core menu  FUNCTION: safe menu  Q: quit";

  if (window == 0) {
    window = 1;
  }
  start = (ui->rom_cursor / window) * window;
  end = start + window;
  if (end > ui->rom_count) {
    end = ui->rom_count;
  }

  if (ui->screen == SCREEN_FAVORITES) {
    title = "FAVORITES";
    subtitle =
        "A: launch  B: TOP  LEFT/RIGHT: page  START: menu  SELECT: core menu  FUNCTION: safe menu  Q: quit";
  } else if (ui->screen == SCREEN_RECENT) {
    title = "RECENT";
    subtitle =
        "A: resume  B: TOP  LEFT/RIGHT: page  START: menu  SELECT: core menu  FUNCTION: safe menu  Q: quit";
  }

  if (ui_uses_graphic_mode(ui)) {
    render_roms_graphic(ui, title, start, end);
    return;
  }

  ui_printf(ui, "plumOS controller UI - %s\n", title);
  if (ui->screen == SCREEN_ROMS) {
    char prompt_path[PATH_MAX];
    ui_printf(ui, "system=%s ROMs=%zu (%s)\n", ui->current_system_id,
              ui->rom_count, ui->current_system_name);
    if (ui->renderer_mali && ui->rom_count > 0 &&
        rom_entry_alias_root_path(&ui->rom_entries[ui->rom_cursor],
                                  prompt_path, sizeof(prompt_path))) {
      ui_printf(ui, "prompt_path=%s\n", prompt_path);
    }
  }
  ui_printf(ui, "%s\n", subtitle);
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->rom_count,
            ui->rom_count ? ui->rom_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = start; i < end; i++) {
    const struct rom_entry *entry = &ui->rom_entries[i];
    const char *detail = entry->detail[0] ? entry->detail : entry->relative_path;
    if (ui->renderer_mali) {
      ui_printf(ui, "%c %3zu  %s\n",
                i == ui->rom_cursor ? '>' : ' ', i + 1, entry->title);
    } else {
      ui_printf(ui, "%c %3zu  %-30s %s\n",
                i == ui->rom_cursor ? '>' : ' ', i + 1, entry->title, detail);
    }
  }
  if (ui->rom_count == 0) {
    ui_printf(ui, "(entry is empty)\n");
  }
  if (ui->screen == SCREEN_FAVORITES) {
    ui_printf(ui, "\nsource: %s\n", ui->favorites_path);
  } else if (ui->screen == SCREEN_RECENT) {
    ui_printf(ui, "\nsource: %s\n", ui->recent_path);
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_start_menu(struct ui_state *ui) {
  size_t i;

  trigger_sdcard_cleanup_from_start_menu(ui);

  ui_printf(ui, "plumOS controller UI - %s\n",
            ui->menu_title[0] ? ui->menu_title : "START");
  ui_printf(ui, "A: open/run  B: back  UP/DOWN: move  Q: quit\n");
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->menu_count,
            ui->menu_count ? ui->menu_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = 0; i < ui->menu_count; i++) {
    const struct menu_entry *entry = &ui->menu_entries[i];
    if (ui->renderer_mali) {
      ui_printf(ui, "%c %2zu  %s%s\n",
                i == ui->menu_cursor ? '>' : ' ', i + 1, entry->display_name,
                entry->confirm ? " confirm" : "");
    } else {
      ui_printf(ui, "%c %3zu  %-24s %-10s %-24s %s\n",
                i == ui->menu_cursor ? '>' : ' ', i + 1, entry->display_name,
                entry->kind[0] ? entry->kind : "-", entry->action,
                entry->confirm ? "confirm" : "");
    }
  }
  if (ui->menu_count == 0) {
    ui_printf(ui, "(menu entry is empty)\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static int setting_value_is_true(const char *value) {
  return value && (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
                   strcmp(value, "on") == 0 || strcmp(value, "enabled") == 0 ||
                   strcmp(value, "Enable") == 0 || strcmp(value, "Enabled") == 0);
}

static int settings_blink_arrow_active(const struct ui_state *ui, size_t row,
                                       int direction) {
  long long now;

  if (!ui || ui->settings_blink_direction != direction ||
      ui->settings_blink_cursor != row) {
    return 0;
  }
  now = current_time_ms();
  if (now >= ui->settings_blink_until_ms) {
    return 0;
  }
  return 1;
}

static void format_setting_row_mali(const struct ui_state *ui, const struct setting_entry *entry,
                                    size_t row, char *out, size_t out_size) {
  enum setting_control_type control = setting_control_type_for_id(entry ? entry->id : NULL);
  int flash_direction = 0;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!entry) {
    return;
  }
  if (control == SETTING_CONTROL_CHECKBOX) {
    snprintf(out, out_size, "[%c] %s",
             setting_value_is_true(entry->value) ? 'x' : ' ', entry->display_name);
    return;
  }
  if (control == SETTING_CONTROL_CHOICE || control == SETTING_CONTROL_NUMBER) {
    if (settings_blink_arrow_active(ui, row, -1)) {
      flash_direction = -1;
    } else if (settings_blink_arrow_active(ui, row, 1)) {
      flash_direction = 1;
    }
    if (flash_direction != 0) {
      snprintf(out, out_size, "%s < %s >%s%c}", entry->display_name, entry->value,
               PLUMOS_MALI_SETTING_FLASH_MARKER, flash_direction < 0 ? 'L' : 'R');
    } else {
      snprintf(out, out_size, "%s < %s >", entry->display_name, entry->value);
    }
    return;
  }
  if (control == SETTING_CONTROL_ACTION) {
    snprintf(out, out_size, "%s", entry->display_name);
    return;
  }
  if (entry->value[0] && strlen(entry->display_name) + 2 + strlen(entry->value) <= 33) {
    snprintf(out, out_size, "%s: %s", entry->display_name, entry->value);
  } else {
    snprintf(out, out_size, "%s", entry->display_name);
  }
}

static void setting_help_lines(const struct setting_entry *entry,
                               char *line1, size_t line1_size,
                               char *line2, size_t line2_size) {
  const char *id = entry ? entry->id : "";
  enum setting_control_type control = setting_control_type_for_id(id);

  if (line1 && line1_size > 0) {
    line1[0] = '\0';
  }
  if (line2 && line2_size > 0) {
    line2[0] = '\0';
  }
  if (!entry) {
    return;
  }

  if (strcmp(id, "ui_mode") == 0) {
    copy_string(line1, line1_size, "Switch all screens: Text or Graphic.");
    copy_string(line2, line2_size, "Text is console-style; Graphic is artwork-focused.");
  } else if (strcmp(id, "show_empty_systems") == 0) {
    copy_string(line1, line1_size, "Show systems with no ROMs.");
    copy_string(line2, line2_size, "Useful while preparing folders or testing aliases.");
  } else if (strcmp(id, "show_favorites_on_top") == 0) {
    copy_string(line1, line1_size, "Show Favorites on the TOP list.");
    copy_string(line2, line2_size, "Favorites behave like a virtual system.");
  } else if (strcmp(id, "show_recent_on_top") == 0) {
    copy_string(line1, line1_size, "Show Recent on the TOP list.");
    copy_string(line2, line2_size, "Recent behaves like a virtual system.");
  } else if (strcmp(id, "boot_resume_mode") == 0) {
    copy_string(line1, line1_size, "Choose startup resume behavior.");
    copy_string(line2, line2_size, "Off ignores; Last resumes; Picker asks.");
  } else if (strcmp(id, "sort_systems") == 0) {
    copy_string(line1, line1_size, "Choose TOP system ordering.");
    copy_string(line2, line2_size, "Sort Order follows config; Name sorts A-Z.");
  } else if (strcmp(id, "sort_roms") == 0) {
    copy_string(line1, line1_size, "Choose ROM list ordering.");
    copy_string(line2, line2_size, "Affects each system's ROM list.");
  } else if (strcmp(id, "rom_scan_policy") == 0) {
    copy_string(line1, line1_size, "Scan ROM folders when entering a system.");
    copy_string(line2, line2_size, "Off keeps cached lists until refresh.");
  } else if (strcmp(id, "ui_theme_settings") == 0) {
    copy_string(line1, line1_size, "Open Graphic theme settings.");
    copy_string(line2, line2_size, "Theme selection affects Graphic mode only.");
  } else if (strcmp(id, "graphic_theme_id") == 0) {
    copy_string(line1, line1_size, "Graphic mode theme package.");
    copy_string(line2, line2_size, "Text mode ignores theme colors, layout, and assets.");
  } else if (strcmp(id, "theme_top_layout") == 0) {
    copy_string(line1, line1_size, "Graphic TOP tile layout.");
    copy_string(line2, line2_size, "Grid shows 6 tiles; Strip shows 2 wide tiles.");
  } else if (strcmp(id, "theme_transition") == 0) {
    copy_string(line1, line1_size, "Graphic TOP page transition.");
    copy_string(line2, line2_size, "Presentation only; input behavior does not change.");
  } else if (strcmp(id, "theme_transition_ms") == 0) {
    copy_string(line1, line1_size, "Graphic TOP transition duration.");
    copy_string(line2, line2_size, "LEFT/RIGHT changes 80..1000 ms.");
  } else if (strcmp(id, "theme_transition_axis") == 0) {
    copy_string(line1, line1_size, "Graphic TOP slide direction.");
    copy_string(line2, line2_size, "Vertical or Horizontal animation axis.");
  } else if (strcmp(id, "theme_transition_easing") == 0) {
    copy_string(line1, line1_size, "Graphic TOP motion curve.");
    copy_string(line2, line2_size, "Smooth is eased; Linear is constant speed.");
  } else if (strcmp(id, "theme_name") == 0 ||
             strcmp(id, "theme_status") == 0 ||
             strcmp(id, "theme_layout") == 0 ||
             strcmp(id, "theme_font") == 0) {
    copy_string(line1, line1_size, "Read-only Graphic theme information.");
    copy_string(line2, line2_size, "Theme package metadata; values above can override motion.");
  } else if (strcmp(id, "rom_scan_slow_threshold_ms") == 0) {
    copy_string(line1, line1_size, "Slow scan warning threshold.");
    copy_string(line2, line2_size, "Higher values make warnings less sensitive.");
  } else if (strcmp(id, "rom_scan_test_file_count") == 0) {
    copy_string(line1, line1_size, "Synthetic scan test count.");
    copy_string(line2, line2_size, "Used by scan performance checks.");
  } else if (strcmp(id, "network_connect_wifi") == 0) {
    copy_string(line1, line1_size, "Scan SSIDs and connect with password.");
    copy_string(line2, line2_size, "Saves Wi-Fi config only after IP is acquired.");
  } else if (strcmp(id, "network_rescue") == 0) {
    copy_string(line1, line1_size, "Network Recovery is disabled.");
    copy_string(line2, line2_size, "Use Connect Wi-Fi or NW Service instead.");
  } else if (strcmp(id, "network_services") == 0) {
    copy_string(line1, line1_size, "Open network services.");
    copy_string(line2, line2_size, "SSH, FTP, SFTP, Samba, and USB Disk Mode.");
  } else if (strcmp(id, "network_usb_disk_mode") == 0) {
    copy_string(line1, line1_size, "Expose the SD card as a USB drive.");
    copy_string(line2, line2_size, "Requires PC eject and USB disconnect to return.");
  } else if (strcmp(id, "network_information") == 0) {
    copy_string(line1, line1_size, "Open read-only network information.");
    copy_string(line2, line2_size, "Connection, IP, signal, link speed, and SSH.");
  } else if (strncmp(id, "network_", 8) == 0) {
    if (strcmp(id, "network_wifi_enabled") == 0) {
      copy_string(line1, line1_size, "Turn the Wi-Fi runtime on or off.");
      copy_string(line2, line2_size, "Use Connect Wi-Fi to start a new connection.");
    } else if (strcmp(id, "network_ssh_enabled") == 0) {
      copy_string(line1, line1_size, "SSH remote shell service.");
      copy_string(line2, line2_size, "Port 2222; SFTP depends on this service.");
    } else if (strcmp(id, "network_ftp_enabled") == 0) {
      copy_string(line1, line1_size, "FTP file transfer service.");
      copy_string(line2, line2_size, "Home is /mnt/SDCARD; ON/OFF persists after reboot.");
    } else if (strcmp(id, "network_sftp_enabled") == 0) {
      copy_string(line1, line1_size, "SFTP file transfer over SSH.");
      copy_string(line2, line2_size, "Home is /mnt/SDCARD; ON/OFF persists after reboot.");
    } else if (strcmp(id, "network_samba_enabled") == 0) {
      copy_string(line1, line1_size, "Windows/macOS network drive service.");
      copy_string(line2, line2_size, "Share is SDCARD; ON/OFF persists after reboot.");
    } else if (strcmp(id, "network_ftp_status") == 0 ||
               strcmp(id, "network_sftp_status") == 0 ||
               strcmp(id, "network_samba_status") == 0) {
      copy_string(line1, line1_size, "Current file transfer service status.");
      copy_string(line2, line2_size, "All services use /mnt/SDCARD as the home/share root.");
    } else if (strcmp(id, "network_config_source") == 0) {
      copy_string(line1, line1_size, "Read-only Wi-Fi config inventory.");
      copy_string(line2, line2_size, "Credential editing waits for backup and rollback.");
    } else if (strcmp(id, "network_connection") == 0 ||
               strcmp(id, "network_ip_address") == 0 ||
               strcmp(id, "network_signal") == 0 ||
               strcmp(id, "network_link_speed") == 0 ||
               strcmp(id, "network_frequency") == 0 ||
               strcmp(id, "network_status_source") == 0) {
      copy_string(line1, line1_size, "Current Wi-Fi connection information.");
      copy_string(line2, line2_size, "Use Connect Wi-Fi if this looks wrong.");
    } else if (strcmp(id, "network_ssh") == 0) {
      copy_string(line1, line1_size, "Remote access path used for development.");
      copy_string(line2, line2_size, "SFTP uses the same SSH service.");
    } else if (strcmp(id, "network_credentials") == 0) {
      copy_string(line1, line1_size, "Wi-Fi credentials are intentionally hidden.");
      copy_string(line2, line2_size, "Do not expose SSID or PSK in UI/logs/git.");
    } else if (strcmp(id, "network_runtime_control") == 0) {
      copy_string(line1, line1_size, "Runtime control helper used by this screen.");
      copy_string(line2, line2_size, "It does not edit saved Wi-Fi credentials.");
    } else {
      copy_string(line1, line1_size, "Read-only network setting.");
      copy_string(line2, line2_size, "Write support needs safe Wi-Fi editor flow.");
    }
  } else if (strncmp(id, "performance_", 12) == 0) {
    if (strcmp(id, "performance_system") == 0) {
      copy_string(line1, line1_size, "Choose the system CPU setting to edit.");
      copy_string(line2, line2_size, "Values are saved as plumOS core overrides.");
    } else if (strcmp(id, "performance_cpu_policy") == 0) {
      copy_string(line1, line1_size, "Set the fixed CPU frequency for this system.");
      copy_string(line2, line2_size, "Predictable presets: 648, 816, 1200, or 1344 MHz.");
    } else if (strcmp(id, "performance_cpu_cores") == 0) {
      copy_string(line1, line1_size, "Set the CPU core count for this system.");
      copy_string(line2, line2_size, "A30 presets are 2 cores or 4 cores.");
    } else if (strcmp(id, "performance_clear_cpu_override") == 0) {
      copy_string(line1, line1_size, "Reset this system to the plumOS default.");
      copy_string(line2, line2_size, "Default is 648 MHz and 2 cores.");
    } else {
      copy_string(line1, line1_size, "Read-only performance source information.");
      copy_string(line2, line2_size, "Shows whether defaults or overrides are active.");
    }
  } else if (strcmp(id, "system_information") == 0) {
    copy_string(line1, line1_size, "Open read-only device information.");
    copy_string(line2, line2_size, "Kernel, storage, input, backend, and policy details.");
  } else if (strncmp(id, "system_", 7) == 0) {
    if (strcmp(id, "system_volume") == 0) {
      copy_string(line1, line1_size, "System-wide volume setting.");
      copy_string(line2, line2_size, "Applies to ALSA Soft Volume Master.");
    } else if (strcmp(id, "system_brightness") == 0) {
      copy_string(line1, line1_size, "Screen brightness setting.");
      copy_string(line2, line2_size, "LEFT/RIGHT changes 1..20 and applies lcdbl.");
    } else if (strcmp(id, "system_lumination") == 0) {
      copy_string(line1, line1_size, "Display lumination setting.");
      copy_string(line2, line2_size, "Applies to A30 enhance and saves to plumOS.");
    } else if (strcmp(id, "system_display_color") == 0) {
      copy_string(line1, line1_size, "Open display color tuning.");
      copy_string(line2, line2_size, "Contrast, hue, and saturation are changed inside.");
    } else if (strcmp(id, "system_time_settings") == 0) {
      copy_string(line1, line1_size, "Open OS time and timezone settings.");
      copy_string(line2, line2_size, "Timezone is saved in plumOS and applied to /etc/TZ.");
    } else if (strcmp(id, "system_current_time") == 0) {
      copy_string(line1, line1_size, "Current OS time in the selected timezone.");
      copy_string(line2, line2_size, "NTP normally keeps the underlying clock correct.");
    } else if (strcmp(id, "system_timezone") == 0) {
      copy_string(line1, line1_size, "OS runtime timezone.");
      copy_string(line2, line2_size, "LEFT/RIGHT saves and applies it immediately.");
    } else if (strcmp(id, "system_manual_time") == 0) {
      copy_string(line1, line1_size, "Open manual time editor.");
      copy_string(line2, line2_size, "Values are interpreted in the selected timezone.");
    } else if (strcmp(id, "system_manual_time_apply") == 0) {
      copy_string(line1, line1_size, "Apply the manual OS time.");
      copy_string(line2, line2_size, "A stops plumOS NTP for this session, then sets time.");
    } else if (strncmp(id, "system_manual_time_", 19) == 0) {
      copy_string(line1, line1_size, "Manual time field.");
      copy_string(line2, line2_size, "LEFT/RIGHT changes the value; A applies at bottom.");
    } else if (strcmp(id, "system_contrast") == 0) {
      copy_string(line1, line1_size, "Display contrast setting.");
      copy_string(line2, line2_size, "Applies to A30 enhance and saves to plumOS.");
    } else if (strcmp(id, "system_hue") == 0) {
      copy_string(line1, line1_size, "Display hue setting.");
      copy_string(line2, line2_size, "Applies to A30 enhance and saves to plumOS.");
    } else if (strcmp(id, "system_saturation") == 0) {
      copy_string(line1, line1_size, "Display saturation setting.");
      copy_string(line2, line2_size, "Applies to A30 enhance and saves to plumOS.");
    } else if (strcmp(id, "system_language") == 0) {
      copy_string(line1, line1_size, "Frontend language setting.");
      copy_string(line2, line2_size, "Saves language to plumOS config.");
    } else if (strcmp(id, "system_model") == 0 || strcmp(id, "system_kernel") == 0 ||
        strcmp(id, "system_sdcard") == 0 || strcmp(id, "system_config") == 0) {
      copy_string(line1, line1_size, "Read-only device and runtime information.");
      copy_string(line2, line2_size, "Used to confirm the active A30 environment.");
    } else if (strcmp(id, "system_audio_backend") == 0) {
      copy_string(line1, line1_size, "Audio backend policy for plumOS.");
      copy_string(line2, line2_size, "Volume writes use the detected mixer backend.");
    } else if (strcmp(id, "system_brightness") == 0 ||
               strcmp(id, "system_lumination") == 0 ||
               strcmp(id, "system_display_color") == 0 ||
               strcmp(id, "system_display_backend") == 0) {
      copy_string(line1, line1_size, "Display backend policy for plumOS.");
      copy_string(line2, line2_size, "Display writes use the detected A30 sysfs backend.");
    } else if (strcmp(id, "system_language") == 0 ||
               strcmp(id, "system_input_device") == 0) {
      copy_string(line1, line1_size, "plumOS-owned system preference inventory.");
      copy_string(line2, line2_size, "stockOS settings are intentionally separate.");
    } else {
      copy_string(line1, line1_size, "Read-only A30 system setting.");
      copy_string(line2, line2_size, "Write support needs backup and rollback.");
    }
  } else if (control == SETTING_CONTROL_CHECKBOX && !setting_is_writable(id)) {
    copy_string(line1, line1_size, "Read-only checkbox from theme/system.");
    copy_string(line2, line2_size, "Needs a validated write backend.");
  } else {
    copy_string(line1, line1_size, "Read-only information for this screen.");
    copy_string(line2, line2_size, "Write support will be added after backend checks.");
  }
}

static size_t brightness_test_nearest_index(long brightness) {
  size_t i;
  size_t best = 0;
  long best_delta = brightness > BRIGHTNESS_TEST_VALUES[0]
                        ? brightness - BRIGHTNESS_TEST_VALUES[0]
                        : BRIGHTNESS_TEST_VALUES[0] - brightness;

  for (i = 1; i < BRIGHTNESS_TEST_COUNT; i++) {
    long value = BRIGHTNESS_TEST_VALUES[i];
    long delta = brightness > value ? brightness - value : value - brightness;
    if (delta < best_delta) {
      best = i;
      best_delta = delta;
    }
  }
  return best;
}

static void render_brightness_test_settings(struct ui_state *ui) {
  size_t i;
  long current = brightness_raw_value(ui->device.brightness);
  long selected = current;

  if (ui->setting_count > 0 && ui->settings_cursor < ui->setting_count) {
    selected = BRIGHTNESS_TEST_VALUES[ui->settings_cursor];
  }

  ui_printf(ui, "plumOS controller UI - %s\n",
            settings_category_title(ui->settings_category));
  ui_printf(ui, "A: apply  B: back  UP/DOWN/LEFT/RIGHT: move  Q: quit\n");
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->setting_count,
            ui->setting_count ? ui->settings_cursor + 1 : 0);
  ui_printf(ui, "brightness_current=%ld\n", current);
  ui_printf(ui, "\n");

  if (ui->renderer_mali) {
    for (i = 0; i < BRIGHTNESS_TEST_COUNT; i++) {
      long value = BRIGHTNESS_TEST_VALUES[i];
      ui_printf(ui, "brightness_tile=%ld selected=%d current=%d\n",
                value, i == ui->settings_cursor ? 1 : 0,
                value == current ? 1 : 0);
    }
    ui_printf(ui, "footer1=Current brightness: %ld\n", current);
    ui_printf(ui, "footer2=Selected preset: %ld\n", selected);
  } else {
    for (i = 0; i < BRIGHTNESS_TEST_COUNT; i++) {
      long value = BRIGHTNESS_TEST_VALUES[i];
      if (i % BRIGHTNESS_TEST_COLUMNS == 0) {
        ui_printf(ui, "  ");
      }
      ui_printf(ui, "[%c%3ld%s]", i == ui->settings_cursor ? '>' : ' ',
                value, value == current ? "*" : " ");
      if (i % BRIGHTNESS_TEST_COLUMNS == BRIGHTNESS_TEST_COLUMNS - 1 ||
          i + 1 == BRIGHTNESS_TEST_COUNT) {
        ui_printf(ui, "\n");
      } else {
        ui_printf(ui, "  ");
      }
    }
    ui_printf(ui, "\ncurrent=%ld selected=%ld\n", current, selected);
  }

  ui_printf(ui, "\nsource: %s\n", ui->system_config_path);
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_settings(struct ui_state *ui) {
  size_t i;
  size_t window = ui_list_window_size(ui);
  size_t start = 0;
  size_t end;
  char help1[128];
  char help2[128];

  if (window == 0) {
    window = 1;
  }
  if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST) {
    render_brightness_test_settings(ui);
    return;
  }
  start = (ui->settings_cursor / window) * window;
  end = start + window;
  if (end > ui->setting_count) {
    end = ui->setting_count;
  }

  ui_printf(ui, "plumOS controller UI - %s\n", settings_category_title(ui->settings_category));
  ui_printf(ui, "A: toggle/run  B: back  LEFT/RIGHT: change  UP/DOWN: move  Q: quit\n");
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->setting_count,
            ui->setting_count ? ui->settings_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = start; i < end; i++) {
    const struct setting_entry *entry = &ui->setting_entries[i];
    if (ui->renderer_mali) {
      char row[256];
      format_setting_row_mali(ui, entry, i, row, sizeof(row));
      ui_printf(ui, "%c %3zu  %s\n",
                i == ui->settings_cursor ? '>' : ' ', i + 1, row);
    } else {
      enum setting_control_type control = setting_control_type_for_id(entry->id);
      if (control == SETTING_CONTROL_ACTION) {
        ui_printf(ui, "%c %3zu  %s\n",
                  i == ui->settings_cursor ? '>' : ' ', i + 1,
                  entry->display_name);
      } else {
        ui_printf(ui, "%c %3zu  %-24s %s\n",
                  i == ui->settings_cursor ? '>' : ' ', i + 1,
                  entry->display_name, entry->value);
      }
    }
  }
  if (ui->renderer_mali && ui->setting_count > 0) {
    const struct setting_entry *entry = &ui->setting_entries[ui->settings_cursor];
    setting_help_lines(entry, help1, sizeof(help1), help2, sizeof(help2));
    ui_printf(ui, "footer1=%s\n", help1);
    ui_printf(ui, "footer2=%s\n", help2);
  }
  ui_printf(ui, "\nsource: %s\n", ui->settings_path);
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_safe_menu(struct ui_state *ui) {
  size_t i;

  ui_printf(ui, "plumOS controller UI - SAFE\n");
  ui_printf(ui, "A: run  B: cancel  UP/DOWN: move  Q: quit\n");
  if (ui->safe_target_relative_path[0]) {
    ui_printf(ui, "target=%s / %s\n", ui->safe_target_system_id,
              ui->safe_target_relative_path);
    ui_printf(ui, "profile=%s\n",
              ui->safe_target_launch_profile[0] ? ui->safe_target_launch_profile : "-");
  } else {
    ui_printf(ui, "target=(no active ROM target)\n");
  }
  ui_printf(ui, "entries=%zu cursor=%zu\n", SAFE_ENTRY_COUNT,
            SAFE_ENTRY_COUNT ? ui->safe_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = 0; i < SAFE_ENTRY_COUNT; i++) {
    const struct safe_entry *entry = &SAFE_ENTRIES[i];
    ui_printf(ui, "%c %3zu  %-12s %s\n",
              i == ui->safe_cursor ? '>' : ' ', i + 1, entry->display_name, entry->detail);
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_help(struct ui_state *ui) {
  ui_printf(ui, "plumOS controller UI - HELP\n");
  ui_printf(ui, "B: back  Q: quit\n");
  ui_printf(ui, "entries=9 cursor=0\n");
  ui_printf(ui, "\n");
  ui_printf(ui, "1. Up / Down: Move cursor\n");
  ui_printf(ui, "2. A: Open or run\n");
  ui_printf(ui, "3. B: Back or cancel\n");
  ui_printf(ui, "4. START: Open START menu\n");
  ui_printf(ui, "5. SELECT: Core menu\n");
  ui_printf(ui, "6. Function: SAFE menu\n");
  ui_printf(ui, "7. Settings HELP: This screen\n");
  ui_printf(ui, "8. Network: Run Wi-Fi and SSH rescue\n");
  ui_printf(ui, "9. Q: Quit from SSH text input only\n");
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_core_select(struct ui_state *ui) {
  size_t i;
  const char *profile = NULL;
  char cores_label[64];
  const char *footer1 = "Launch core/profile used for this target.";
  const char *footer2 = "TOP saves system; ROM saves ROM override.";

  ui_printf(ui, "plumOS controller UI - Core Settings\n");
  ui_printf(ui, "target=%s", ui->core_target_system_id[0] ? ui->core_target_system_id : "-");
  if (ui->core_target_relative_path[0]) {
    ui_printf(ui, " / %s", ui->core_target_relative_path);
  }
  ui_printf(ui, "\n");
  if (ui->core_current_source[0]) {
    ui_printf(ui, "source=%s\n", ui->core_current_source);
  }
  core_menu_clamp_cursor(ui);
  ui_printf(ui, "entries=%d cursor=%zu\n\n", CORE_MENU_ROW_COUNT,
            ui->core_menu_cursor + 1);
  if (ui->core_profile_count > 0) {
    if (ui->core_profile_cursor >= ui->core_profile_count) {
      ui->core_profile_cursor = ui->core_profile_count - 1;
    }
    profile = ui->core_profiles[ui->core_profile_cursor].id;
  } else if (ui->core_current_profile[0]) {
    profile = ui->core_current_profile;
  }
  if (ui->core_cpu_cores > 0) {
    snprintf(cores_label, sizeof(cores_label), "%ld", ui->core_cpu_cores);
  } else {
    copy_string(cores_label, sizeof(cores_label), "launcher default");
  }

  ui_printf(ui, "%c   1  Cores < %s >\n",
            ui->core_menu_cursor == CORE_MENU_ROW_PROFILE ? '>' : ' ',
            core_profile_display_name(profile));
  ui_printf(ui, "    2  ------------------------------\n");
  ui_printf(ui, "%c   3  CPU freq < %s >\n",
            ui->core_menu_cursor == CORE_MENU_ROW_CPU_FREQ ? '>' : ' ',
            ui->core_cpu_label);
  ui_printf(ui, "%c   4  CPU Cores < %s >\n",
            ui->core_menu_cursor == CORE_MENU_ROW_CPU_CORES ? '>' : ' ',
            cores_label);
  for (i = 0; i < ui->core_line_count; i++) {
    ui_printf(ui, "%s\n", ui->core_lines[i]);
  }
  if (ui->core_menu_cursor == CORE_MENU_ROW_CPU_FREQ) {
    footer1 = "Fixed CPU frequency for this target.";
    footer2 = ui->core_cpu_source[0] ? ui->core_cpu_source : "source unavailable";
  } else if (ui->core_menu_cursor == CORE_MENU_ROW_CPU_CORES) {
    footer1 = "CPU core count for this target.";
    footer2 = ui->core_cpu_cores_source[0] ? ui->core_cpu_cores_source
                                           : "source unavailable";
  }
  if (ui->renderer_mali) {
    ui_printf(ui, "footer1=%s\n", footer1);
    ui_printf(ui, "footer2=%s\n", footer2);
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_scraping(struct ui_state *ui) {
  const char *label;
  const struct scraping_kind_choice *kind;
  long rom_count;

  clamp_scraping_menu_cursor(ui);
  clamp_scraping_choice_cursor(ui);
  kind = scraping_selected_kind(ui);
  label = scraping_selected_label(ui);
  rom_count = scraping_selected_rom_count(ui);

  ui_printf(ui, "plumOS controller UI - Scraping\n");
  ui_printf(ui, "UP/DOWN: item  LEFT/RIGHT: change  A: start  SELECT: results  B: Apps  Q: quit\n");
  ui_printf(ui, "entries=7 cursor=%zu\n", ui->scraping_menu_cursor + 1);
  ui_printf(ui, "\n");
  ui_printf(ui, "%c   1  Image < %s >\n",
            ui->scraping_menu_cursor == UI_SCRAPING_FIELD_IMAGE ? '>' : ' ',
            kind->display_name);
  ui_printf(ui, "%c   2  Existing < %s >\n",
            ui->scraping_menu_cursor == UI_SCRAPING_FIELD_EXISTING ? '>' : ' ',
            scraping_existing_label(ui));
  ui_printf(ui, "%c   3  System < %s >\n",
            ui->scraping_menu_cursor == UI_SCRAPING_FIELD_SYSTEM ? '>' : ' ',
            label);
  ui_printf(ui, "    4  Targets: %zu system%s\n",
            ui->scraping_choice_cursor == 0 ? ui->scraping_choice_count : (size_t)1,
            (ui->scraping_choice_cursor == 0 && ui->scraping_choice_count != 1) ? "s" : "");
  ui_printf(ui, "    5  ROMs: %ld\n", rom_count);
  ui_printf(ui, "    6  A starts plan and fetch\n");
  ui_printf(ui, "    7  SELECT opens latest results\n");
  ui_printf(ui, "footer1=%s\n", "Box Art and Title Screen share the same PNG path.");
  ui_printf(ui, "footer2=%s\n", "Replace overwrites current images after successful download.");
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_thumbnail_results(struct ui_state *ui) {
  size_t i;
  size_t window = UI_THUMBNAIL_RESULT_WINDOW;
  size_t start = 0;
  size_t end;

  ui_printf(ui, "plumOS controller UI - Scraping Results\n");
  ui_printf(ui, "UP/DOWN: scroll  LEFT/RIGHT: page  A/SELECT: refresh  B: Apps  Q: quit\n");
  clamp_thumbnail_result_cursor(ui);
  if (ui->thumbnail_result_count > 0) {
    start = (ui->thumbnail_result_cursor / window) * window;
  }
  end = start + window;
  if (end > ui->thumbnail_result_count) {
    end = ui->thumbnail_result_count;
  }
  ui_printf(ui, "entries=%zu cursor=%zu\n", ui->thumbnail_result_count,
            ui->thumbnail_result_count ? ui->thumbnail_result_cursor + 1 : 0);
  ui_printf(ui, "\n");
  for (i = start; i < end; i++) {
    ui_printf(ui, "%c   %2zu  %s\n",
              i == ui->thumbnail_result_cursor ? '>' : ' ',
              i + 1, ui->thumbnail_result_lines[i]);
  }
  if (ui->thumbnail_result_count == 0) {
    ui_printf(ui, "(no thumbnail result yet)\n");
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_thumbnail_running(struct ui_state *ui) {
  const char *title = ui->thumbnail_running_title[0]
                          ? ui->thumbnail_running_title
                          : "Thumbnail Task";
  int scraping = strcmp(title, "Scraping") == 0;
  const char *phase = ui->thumbnail_running_phase[0]
                          ? ui->thumbnail_running_phase
                          : "starting";
  const char *phase_label = phase;
  long percent = -1;

  if (strcmp(phase, "plan") == 0) {
    phase_label = "Plan";
  } else if (strcmp(phase, "fetch") == 0) {
    phase_label = "Fetch";
  } else if (strcmp(phase, "done") == 0) {
    phase_label = "Done";
  }
  if (ui->thumbnail_progress_total > 0) {
    percent = (ui->thumbnail_progress_current * 100) / ui->thumbnail_progress_total;
  }

  ui_printf(ui, "plumOS controller UI - %s Running\n",
            scraping ? "Scraping" : "Thumbnail");
  ui_printf(ui, "thumbnail_running=1\n");
  ui_printf(ui, "thumbnail_running_title=%s\n", title);
  ui_printf(ui, "thumbnail_running_phase=%s\n", phase_label);
  ui_printf(ui, "thumbnail_running_system=%s\n",
            ui->thumbnail_running_system[0] ? ui->thumbnail_running_system : "-");
  ui_printf(ui, "thumbnail_running_progress=%ld/%ld\n",
            ui->thumbnail_progress_current, ui->thumbnail_progress_total);
  ui_printf(ui, "thumbnail_running_stats=%ld/%ld/%ld\n",
            ui->thumbnail_progress_downloaded,
            ui->thumbnail_progress_no_match,
            ui->thumbnail_progress_failed);
  ui_printf(ui, "entries=6 cursor=1\n");
  ui_printf(ui, "\n");
  ui_printf(ui, ">   1  RUNNING: %s\n", title);
  if (percent >= 0) {
    ui_printf(ui, "    2  Progress: %ld / %ld (%ld%%)\n",
              ui->thumbnail_progress_current, ui->thumbnail_progress_total, percent);
  } else {
    ui_printf(ui, "    2  Progress: preparing\n");
  }
  ui_printf(ui, "    3  Phase: %s %s\n", phase_label,
            ui->thumbnail_running_system[0] ? ui->thumbnail_running_system : "-");
  ui_printf(ui, "    4  Saved %ld  NoMatch %ld  Failed %ld\n",
            ui->thumbnail_progress_downloaded,
            ui->thumbnail_progress_no_match,
            ui->thumbnail_progress_failed);
  ui_printf(ui, "    5  Results will open automatically\n");
  ui_printf(ui, "    6  Do not power off\n");
  ui_printf(ui, "footer1=%s\n",
            scraping ? "Scraping is running." : "Thumbnail task is running.");
  ui_printf(ui, "footer2=%s\n", "The latest result will open when it finishes.");
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_network_rescue(struct ui_state *ui) {
  ui_printf(ui, "plumOS controller UI - Network Recovery Disabled\n");
  ui_printf(ui, "B: back  Q: quit\n");
  ui_printf(ui, "target=(disabled compatibility screen)\n");
  ui_printf(ui, "\n");
  ui_printf(ui, "1. Network Recovery is not exposed from FE.\n");
  ui_printf(ui, "2. Use Network Settings > Connect Wi-Fi.\n");
  ui_printf(ui, "3. Use Network Settings > NW Service.\n");
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_wifi_connect(struct ui_state *ui) {
  size_t i;
  size_t window = ui_list_window_size(ui);
  size_t start = 0;
  size_t end = 0;
  char footer1[160];
  char footer2[160];

  if (window == 0) {
    window = 1;
  }
  ui_printf(ui, "plumOS controller UI - Network Settings - Connect Wi-Fi\n");
  if (ui->wifi_stage == WIFI_CONNECT_SELECT) {
    start = (ui->wifi_cursor / window) * window;
    end = start + window;
    if (end > ui->wifi_count) {
      end = ui->wifi_count;
    }
    ui_printf(ui, "A: select  B: back  SELECT: rescan  UP/DOWN: move  Q: quit\n");
    ui_printf(ui, "entries=%zu cursor=%zu\n", ui->wifi_count,
              ui->wifi_count ? ui->wifi_cursor + 1 : 0);
    ui_printf(ui, "\n");
    for (i = start; i < end; i++) {
      const struct wifi_network_entry *entry = &ui->wifi_networks[i];
      ui_printf(ui, "%c %3zu  %s  [%s]  %s dBm\n",
                i == ui->wifi_cursor ? '>' : ' ', i + 1, entry->ssid,
                entry->security, entry->signal);
    }
    if (ui->wifi_count == 0) {
      ui_printf(ui, "(no SSIDs found)\n");
    }
    ui_printf(ui, "footer1=SSID scan results\n");
    ui_printf(ui, "footer2=A selects a network; SELECT rescans.\n");
  } else if (ui->wifi_stage == WIFI_CONNECT_PASSWORD) {
    const char *ssid = ui->wifi_count > 0 && ui->wifi_cursor < ui->wifi_count
                           ? ui->wifi_networks[ui->wifi_cursor].ssid
                           : "-";
    ui_printf(ui, "A: type/run  B: SSID list  UP/DOWN/LEFT/RIGHT: move  Q: quit\n");
    ui_printf(ui, "target=%s\n", ssid);
    ui_printf(ui, "entries=%d cursor=%zu\n", UI_WIFI_KEYBOARD_ROWS,
              ui->wifi_key_row + 1);
    if (ui->renderer_mali) {
      ui_printf(ui, "wifi_keyboard_cursor=%zu,%zu\n", ui->wifi_key_row,
                ui->wifi_key_col);
      ui_printf(ui, "wifi_password=%s\n",
                ui->wifi_password[0] ? ui->wifi_password : "-");
    }
    ui_printf(ui, "\n");
    for (i = 0; i < UI_WIFI_KEYBOARD_ROWS; i++) {
      char row[128];
      wifi_format_keyboard_row(ui, i, row, sizeof(row));
      ui_printf(ui, "%c %3zu  %s\n",
                !ui->renderer_mali && i == ui->wifi_key_row ? '>' : ' ',
                i + 1, row);
    }
    snprintf(footer1, sizeof(footer1), "Password: %s (%zu chars)",
             ui->wifi_password[0] ? ui->wifi_password : "-",
             strlen(ui->wifi_password));
    snprintf(footer2, sizeof(footer2), "SHIFT toggles case. CONNECT runs.");
    ui_printf(ui, "footer1=%s\n", footer1);
    ui_printf(ui, "footer2=%s\n", footer2);
  } else {
    ui_printf(ui, "A: done  B: Network Settings  Q: quit\n");
    ui_printf(ui, "entries=4 cursor=1\n");
    ui_printf(ui, "\n");
    ui_printf(ui, ">   1  %s\n",
              ui->wifi_result_title[0] ? ui->wifi_result_title : "Connection result");
    ui_printf(ui, "    2  IP Address: %s\n",
              ui->wifi_result_ip[0] ? ui->wifi_result_ip : "-");
    ui_printf(ui, "    3  Gateway: %s\n",
              ui->wifi_result_gateway[0] ? ui->wifi_result_gateway : "-");
    ui_printf(ui, "    4  Gateway Ping: %s\n",
              ui->wifi_result_gateway_ping[0] ? ui->wifi_result_gateway_ping : "-");
    ui_printf(ui, "footer1=%s\n",
              ui->wifi_result_success ? "Connection complete." : "Connection failed.");
    ui_printf(ui, "footer2=%s\n",
              ui->wifi_result_stage[0] ? ui->wifi_result_stage : "B returns.");
  }
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_usb_disk_confirm(struct ui_state *ui) {
  ui_printf(ui, "plumOS controller UI - Network Settings - NW Service - USB Disk Mode\n");
  ui_printf(ui, "A: enter  B: NW Service  Q: quit\n");
  ui_printf(ui, "entries=4 cursor=1\n");
  ui_printf(ui, "\n");
  ui_printf(ui, ">   1  READY TO ENTER\n");
  ui_printf(ui, "    2  SD Card: /mnt/SDCARD\n");
  ui_printf(ui, "    3  PC eject is required\n");
  ui_printf(ui, "    4  USB disconnect returns to plumOS\n");
  ui_printf(ui, "footer1=%s\n", "A exposes the SD card as a USB drive.");
  ui_printf(ui, "footer2=%s\n", "Eject on PC, then unplug USB to finish.");
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_usb_disk_starting(struct ui_state *ui) {
  ui_printf(ui, "plumOS controller UI - Network Settings - NW Service - USB Disk Mode\n");
  ui_printf(ui, "usb_disk_starting=1\n");
  ui_printf(ui, "entries=4 cursor=1\n");
  ui_printf(ui, "\n");
  ui_printf(ui, ">   1  USB DISK MODE STARTING\n");
  ui_printf(ui, "    2  Please wait\n");
  ui_printf(ui, "    3  Preparing SD card\n");
  ui_printf(ui, "    4  Do not unplug yet\n");
  ui_printf(ui, "footer1=%s\n", "Starting USB Disk Mode.");
  ui_printf(ui, "footer2=%s\n", "Windows will detect the drive shortly.");
  if (ui->status[0]) {
    ui_printf(ui, "\nstatus: %s\n", ui->status);
  }
}

static void render_ui(struct ui_state *ui) {
  clear_screen(ui);
  if (ui->screen != SCREEN_TOP) {
    ui->top_transition_active = 0;
  }
  if (ui->screen != SCREEN_GALLERY) {
    ui->gallery_transition_active = 0;
    ui->gallery_pending_active = 0;
  }
  if (ui->rescue_network) {
    render_network_rescue(ui);
  } else if (ui->screen == SCREEN_ROMS || ui->screen == SCREEN_FAVORITES ||
      ui->screen == SCREEN_RECENT) {
    render_roms(ui);
  } else if (ui->screen == SCREEN_GALLERY) {
    render_gallery(ui);
  } else if (ui->screen == SCREEN_START_MENU) {
    render_start_menu(ui);
  } else if (ui->screen == SCREEN_SETTINGS) {
    render_settings(ui);
  } else if (ui->screen == SCREEN_SAFE_MENU) {
    render_safe_menu(ui);
  } else if (ui->screen == SCREEN_HELP) {
    render_help(ui);
  } else if (ui->screen == SCREEN_CORE_SELECT) {
    render_core_select(ui);
  } else if (ui->screen == SCREEN_SCRAPING) {
    render_scraping(ui);
  } else if (ui->screen == SCREEN_THUMBNAIL_RESULTS) {
    render_thumbnail_results(ui);
  } else if (ui->screen == SCREEN_THUMBNAIL_RUNNING) {
    render_thumbnail_running(ui);
  } else if (ui->screen == SCREEN_NETWORK_RESCUE) {
    render_network_rescue(ui);
  } else if (ui->screen == SCREEN_WIFI_CONNECT) {
    render_wifi_connect(ui);
  } else if (ui->screen == SCREEN_USB_DISK_CONFIRM) {
    render_usb_disk_confirm(ui);
  } else if (ui->screen == SCREEN_USB_DISK_STARTING) {
    render_usb_disk_starting(ui);
  } else {
    render_top(ui);
  }
  if (ui->renderer_mali) {
#ifdef PLUMOS_ENABLE_MALI_RENDERER
    if (!ui->renderer_active) {
      ui->render_failed = 1;
      return;
    }
    if (!plumos_mali_render_lines(&ui->mali_renderer, ui->render_lines,
                                  ui->render_line_count)) {
      copy_string(ui->status, sizeof(ui->status), "Mali renderer swap failed");
      ui->render_failed = 1;
    }
#endif
  } else {
    fflush(stdout);
  }
}

static void mark_frontend_ready_if_needed(struct ui_state *ui) {
  char content[128];

  if (!ui || ui->fe_ready_flag_written || !ui->renderer_mali ||
      !ui->renderer_active || ui->render_failed || ui->rescue_network) {
    return;
  }
  snprintf(content, sizeof(content), "pid=%ld\nscreen=%d\n",
           (long)getpid(), (int)ui->screen);
  if (write_text_file_create(UI_FE_READY_FLAG_PATH, content)) {
    ui->fe_ready_flag_written = 1;
  }
}

static void set_status(struct ui_state *ui, const char *text) {
  copy_string(ui->status, sizeof(ui->status), text);
}

static void reset_marquee(struct ui_state *ui) {
#ifdef PLUMOS_ENABLE_MALI_RENDERER
  if (ui && ui->renderer_mali && ui->renderer_active) {
    plumos_mali_renderer_reset_marquee(&ui->mali_renderer);
  }
#else
  (void)ui;
#endif
}

static void settle_input_after_child(struct ui_state *ui);

static int init_ui_renderer(struct ui_state *ui) {
  if (!ui->renderer_mali) {
    return 1;
  }
#ifdef PLUMOS_ENABLE_MALI_RENDERER
  if (ui->renderer_active) {
    return 1;
  }
  {
    char render_error[256];
    render_error[0] = '\0';
    if (!plumos_mali_renderer_init(&ui->mali_renderer,
                                   ui->fb_path[0] ? ui->fb_path : "/dev/fb0",
                                   ui->egl_path[0] ? ui->egl_path : "/usr/lib/libEGL.so",
                                   ui->gles_path[0] ? ui->gles_path : "/usr/lib/libGLESv2.so",
                                   ui->mali_rotation, render_error, sizeof(render_error))) {
      snprintf(ui->status, sizeof(ui->status), "Mali renderer init failed: %.200s",
               render_error[0] ? render_error : "-");
      return 0;
    }
    plumos_mali_renderer_set_tty_entry_scale(&ui->mali_renderer,
                                             ui->mali_tty_entry_scale);
    plumos_mali_renderer_reset_marquee(&ui->mali_renderer);
#ifdef PLUMOS_ENABLE_MALI_FREETYPE
    if (ui->mali_font_path[0]) {
      render_error[0] = '\0';
      if (plumos_mali_renderer_load_font(&ui->mali_renderer, ui->mali_font_path,
                                         render_error, sizeof(render_error))) {
        snprintf(ui->status, sizeof(ui->status), "Mali renderer ready font=%.160s",
                 ui->mali_font_path);
      } else {
        snprintf(ui->status, sizeof(ui->status), "Mali font failed: %.180s",
                 render_error[0] ? render_error : ui->mali_font_path);
      }
    }
#endif
  }
  ui->renderer_active = 1;
  return 1;
#else
  set_status(ui, "Mali renderer unavailable in this build");
  return 0;
#endif
}

static void shutdown_ui_renderer(struct ui_state *ui) {
#ifdef PLUMOS_ENABLE_MALI_RENDERER
  if (ui->renderer_mali && ui->renderer_active) {
    plumos_mali_renderer_shutdown(&ui->mali_renderer);
    ui->renderer_active = 0;
  }
#else
  (void)ui;
#endif
}

static void open_start_menu(struct ui_state *ui) {
  ui->back_screen = ui->screen;
  ui->screen = SCREEN_START_MENU;
  trigger_sdcard_cleanup_from_start_menu(ui);
  if (!load_start_menu_entries(ui)) {
    set_status(ui, "cannot load START menu");
  } else {
    set_status(ui, "START menu ready");
  }
}

static void open_apps_menu(struct ui_state *ui) {
  ui->screen = SCREEN_START_MENU;
  trigger_sdcard_cleanup_from_start_menu(ui);
  if (!load_apps_menu_entries(ui)) {
    set_status(ui, "cannot load Apps menu");
  } else {
    set_status(ui, "Apps ready");
  }
}

static void capture_safe_target(struct ui_state *ui) {
  ui->safe_target_system_id[0] = '\0';
  ui->safe_target_relative_path[0] = '\0';
  ui->safe_target_launch_profile[0] = '\0';

  if ((ui->screen == SCREEN_ROMS || ui->screen == SCREEN_FAVORITES ||
       ui->screen == SCREEN_RECENT) &&
      ui->rom_count > 0) {
    const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
    copy_string(ui->safe_target_system_id, sizeof(ui->safe_target_system_id),
                entry->system_id[0] ? entry->system_id : ui->current_system_id);
    copy_string(ui->safe_target_relative_path, sizeof(ui->safe_target_relative_path),
                entry->relative_path);
    copy_string(ui->safe_target_launch_profile, sizeof(ui->safe_target_launch_profile),
                entry->launch_profile);
  }
}

static void close_safe_menu(struct ui_state *ui, const char *status) {
  ui->screen = ui->safe_back_screen;
  set_status(ui, status);
}

static void open_safe_menu(struct ui_state *ui) {
  ui->safe_back_screen = ui->screen;
  ui->safe_cursor = SAFE_ENTRY_COUNT > 0 ? SAFE_ENTRY_COUNT - 1 : 0;
  capture_safe_target(ui);
  ui->screen = SCREEN_SAFE_MENU;
  set_status(ui, "safe menu ready");
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
  reset_marquee(ui);
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
  reset_marquee(ui);
}

static void open_settings_screen(struct ui_state *ui, enum settings_category category) {
  const char *title;

  ui->settings_category = category;
  ui->screen = SCREEN_SETTINGS;
  title = settings_category_title(ui->settings_category);
  if (!load_settings_entries(ui)) {
    snprintf(ui->status, sizeof(ui->status), "cannot load %s", title);
  } else {
    snprintf(ui->status, sizeof(ui->status), "%s ready", title);
  }
}

static void open_system_brightness_test_screen(struct ui_state *ui) {
  open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST);
  if (ui->setting_count > 0) {
    ui->settings_cursor = brightness_test_nearest_index(
        brightness_raw_value(ui->device.brightness));
    if (ui->settings_cursor >= ui->setting_count) {
      ui->settings_cursor = ui->setting_count - 1;
    }
  }
}

static void open_help_screen(struct ui_state *ui) {
  ui->screen = SCREEN_HELP;
  set_status(ui, "help ready");
}

static void open_thumbnail_results_screen(struct ui_state *ui) {
  ui->screen = SCREEN_THUMBNAIL_RESULTS;
  ui->thumbnail_result_cursor = 0;
  if (!load_thumbnail_results(ui)) {
    set_status(ui, "cannot load thumbnail results");
  } else {
    set_status(ui, "scraping results ready");
  }
}

static void open_scraping_screen(struct ui_state *ui) {
  ui->screen = SCREEN_SCRAPING;
  set_status(ui, "refreshing ROM list");
  render_ui(ui);
  if (!run_scanner(ui->plumos_root, ui->sdcard_root, NULL, 0)) {
    set_status(ui, "ROM scan failed; using cached counts");
  } else if (!load_top_entries(ui)) {
    set_status(ui, "ROM scan done; cannot reload TOP");
  }
  if (!load_scraping_choices(ui)) {
    set_status(ui, "cannot load scraping systems");
  } else if (ui->scraping_choice_count == 0) {
    set_status(ui, "no scraping target with ROMs");
  } else {
    set_status(ui, "Scraping ready");
  }
}

static int load_core_select_lines(struct ui_state *ui, const char *system_id,
                                  const char *relative_path) {
  char text_ui[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  char line[512];
  FILE *pipe;
  size_t pos = 0;
  int rc;
  int in_launch_profiles = 0;

  reset_core_profile_choices(ui);
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui")) {
    core_append_line(ui, "error: plumos-text-ui path too long");
    return 0;
  }
  if (!file_exists(text_ui)) {
    core_append_line(ui, "error: plumos-text-ui missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, relative_path && relative_path[0]
                                             ? " core rom "
                                             : " core system ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, system_id)) {
    core_append_line(ui, "error: core command too long");
    return 0;
  }
  if (relative_path && relative_path[0]) {
    if (!append_string(cmd, sizeof(cmd), &pos, " ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, relative_path) ||
        !append_string(cmd, sizeof(cmd), &pos, " --no-scan")) {
      core_append_line(ui, "error: core command too long");
      return 0;
    }
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    core_append_line(ui, "error: core command too long");
    return 0;
  }

  pipe = popen(cmd, "r");
  if (!pipe) {
    core_append_line(ui, "error: cannot run plumos-text-ui core");
    return 0;
  }
  while (fgets(line, sizeof(line), pipe)) {
    trim_line_end(line);
    if (strcmp(line, "Launch profiles") == 0) {
      in_launch_profiles = 1;
      continue;
    }
    if (strcmp(line, "CPU frequency presets") == 0) {
      in_launch_profiles = 0;
      continue;
    }
    parse_core_current_profile_line(ui, line);
    parse_core_current_cpu_line(ui, line);
    parse_core_current_cpu_cores_line(ui, line);
    if (in_launch_profiles && parse_core_profile_choice_line(ui, line)) {
      continue;
    }
    if (strncmp(line, "error:", 6) == 0) {
      core_append_line(ui, line);
    }
  }
  rc = pclose(pipe);
  select_current_core_profile(ui);
  if (rc == 0 && ui->core_profile_count == 0 && ui->core_line_count == 0) {
    core_append_line(ui, "no launch profiles for this system");
  }
  return rc == 0;
}

static int run_core_text_ui_extra(struct ui_state *ui, const char *extra_args) {
  char text_ui[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!ui) {
    return 0;
  }
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui")) {
    set_status(ui, "core command path too long");
    return 0;
  }
  if (!file_exists(text_ui)) {
    set_status(ui, "plumos-text-ui missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos,
                     ui->core_target_relative_path[0] ? " core rom "
                                                      : " core system ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->core_target_system_id)) {
    set_status(ui, "core command too long");
    return 0;
  }
  if (ui->core_target_relative_path[0]) {
    if (!append_string(cmd, sizeof(cmd), &pos, " ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->core_target_relative_path)) {
      set_status(ui, "core command too long");
      return 0;
    }
  }
  if (!append_string(cmd, sizeof(cmd), &pos, extra_args ? extra_args : "") ||
      (ui->core_target_relative_path[0] &&
       !append_string(cmd, sizeof(cmd), &pos, " --no-scan")) ||
      !append_string(cmd, sizeof(cmd), &pos, " >/dev/null 2>&1")) {
    set_status(ui, "core command too long");
    return 0;
  }

  rc = system(cmd);
  if (system_command_succeeded(rc)) {
    return 1;
  }
  set_status(ui, rc == -1 ? "core command failed to start" : "core command returned non-zero");
  return 0;
}

static int run_core_set_profile(struct ui_state *ui, const char *profile) {
  char extra[192];
  size_t pos = 0;

  if (!ui || !valid_launch_profile_id(profile)) {
    return 0;
  }
  extra[0] = '\0';
  if (!append_string(extra, sizeof(extra), &pos, " --set ") ||
      !append_shell_quoted(extra, sizeof(extra), &pos, profile)) {
    set_status(ui, "core command too long");
    return 0;
  }
  return run_core_text_ui_extra(ui, extra);
}

static void cycle_core_profile(struct ui_state *ui, int direction) {
  const char *profile;
  char label[128];

  if (!ui || direction == 0) {
    return;
  }
  if (ui->core_profile_count == 0) {
    set_status(ui, "no core choices");
    return;
  }
  if (ui->core_profile_count == 1) {
    set_status(ui, "only one core choice");
    return;
  }
  if (direction > 0) {
    ui->core_profile_cursor = (ui->core_profile_cursor + 1) % ui->core_profile_count;
  } else if (ui->core_profile_cursor == 0) {
    ui->core_profile_cursor = ui->core_profile_count - 1;
  } else {
    ui->core_profile_cursor--;
  }
  profile = ui->core_profiles[ui->core_profile_cursor].id;
  copy_truncated_string(label, sizeof(label), core_profile_display_name(profile));
  if (!run_core_set_profile(ui, profile)) {
    return;
  }
  if (!load_core_select_lines(ui, ui->core_target_system_id,
                              ui->core_target_relative_path[0]
                                  ? ui->core_target_relative_path
                                  : NULL)) {
    set_status(ui, "core saved; reload failed");
    return;
  }
  snprintf(ui->status, sizeof(ui->status), "Cores saved: %.80s", label);
}

static void cycle_core_cpu_policy(struct ui_state *ui, int direction) {
  const struct performance_cpu_preset *preset;
  char extra[128];
  int index;

  if (!ui || direction == 0) {
    return;
  }
  if (!load_core_select_lines(ui, ui->core_target_system_id,
                              ui->core_target_relative_path[0]
                                  ? ui->core_target_relative_path
                                  : NULL)) {
    set_status(ui, "cannot refresh CPU state");
    return;
  }
  index = performance_cpu_preset_index(ui->core_cpu_policy,
                                       ui->core_cpu_freq_khz);
  index += direction > 0 ? 1 : -1;
  if (index < 0) {
    index = (int)PERFORMANCE_CPU_PRESET_COUNT - 1;
  } else if ((size_t)index >= PERFORMANCE_CPU_PRESET_COUNT) {
    index = 0;
  }
  preset = &PERFORMANCE_CPU_PRESETS[index];
  if (strcmp(preset->policy, "fixed") == 0) {
    snprintf(extra, sizeof(extra), " --cpu fixed --freq %ld", preset->freq_khz);
  } else {
    snprintf(extra, sizeof(extra), " --cpu %s", preset->policy);
  }
  if (!run_core_text_ui_extra(ui, extra)) {
    return;
  }
  if (!load_core_select_lines(ui, ui->core_target_system_id,
                              ui->core_target_relative_path[0]
                                  ? ui->core_target_relative_path
                                  : NULL)) {
    set_status(ui, "CPU freq saved; reload failed");
    return;
  }
  snprintf(ui->status, sizeof(ui->status), "CPU freq saved: %.80s",
           preset->label);
}

static void cycle_core_cpu_cores(struct ui_state *ui, int direction) {
  size_t i;
  size_t index = 0;
  char extra[64];
  long cores;

  if (!ui || direction == 0) {
    return;
  }
  if (!load_core_select_lines(ui, ui->core_target_system_id,
                              ui->core_target_relative_path[0]
                                  ? ui->core_target_relative_path
                                  : NULL)) {
    set_status(ui, "cannot refresh CPU core state");
    return;
  }
  for (i = 0; i < PERFORMANCE_CPU_CORE_PRESET_COUNT; i++) {
    if (ui->core_cpu_cores == PERFORMANCE_CPU_CORE_PRESETS[i]) {
      index = i;
      break;
    }
  }
  if (direction > 0) {
    index = (index + 1) % PERFORMANCE_CPU_CORE_PRESET_COUNT;
  } else if (index == 0) {
    index = PERFORMANCE_CPU_CORE_PRESET_COUNT - 1;
  } else {
    index--;
  }
  cores = PERFORMANCE_CPU_CORE_PRESETS[index];
  snprintf(extra, sizeof(extra), " --cores %ld", cores);
  if (!run_core_text_ui_extra(ui, extra)) {
    return;
  }
  if (!load_core_select_lines(ui, ui->core_target_system_id,
                              ui->core_target_relative_path[0]
                                  ? ui->core_target_relative_path
                                  : NULL)) {
    set_status(ui, "CPU Cores saved; reload failed");
    return;
  }
  snprintf(ui->status, sizeof(ui->status), "CPU Cores saved: %ld", cores);
}

static void cycle_core_menu_current_row(struct ui_state *ui, int direction) {
  if (!ui || direction == 0) {
    return;
  }
  core_menu_clamp_cursor(ui);
  if (ui->core_menu_cursor == CORE_MENU_ROW_PROFILE) {
    cycle_core_profile(ui, direction);
  } else if (ui->core_menu_cursor == CORE_MENU_ROW_CPU_FREQ) {
    cycle_core_cpu_policy(ui, direction);
  } else if (ui->core_menu_cursor == CORE_MENU_ROW_CPU_CORES) {
    cycle_core_cpu_cores(ui, direction);
  }
}

static void open_core_select_screen(struct ui_state *ui, const char *system_id,
                                    const char *relative_path) {
  ui->core_back_screen = ui->screen;
  ui->screen = SCREEN_CORE_SELECT;
  ui->core_menu_cursor = CORE_MENU_ROW_PROFILE;
  copy_string(ui->core_target_system_id, sizeof(ui->core_target_system_id), system_id);
  copy_string(ui->core_target_relative_path, sizeof(ui->core_target_relative_path),
              relative_path ? relative_path : "");
  if (!load_core_select_lines(ui, system_id, relative_path)) {
    set_status(ui, "cannot load core selection");
  } else {
    set_status(ui, "core selection ready");
  }
}

static void open_usb_disk_confirm_screen(struct ui_state *ui) {
  ui->screen = SCREEN_USB_DISK_CONFIRM;
  ui->usb_disk_start_due_ms = 0;
  set_status(ui, "USB Disk Mode confirmation");
}

static void open_usb_disk_starting_screen(struct ui_state *ui) {
  ui->screen = SCREEN_USB_DISK_STARTING;
  ui->usb_disk_start_due_ms = current_time_ms() + UI_USB_DISK_START_DELAY_MS;
  set_status(ui, "USB Disk Mode starting");
}

static void open_rom_screen(struct ui_state *ui, const struct top_entry *entry) {
  copy_string(ui->current_system_id, sizeof(ui->current_system_id), entry->id);
  copy_string(ui->current_system_name, sizeof(ui->current_system_name), entry->display_name);
  if (strcmp(entry->id, "favorites") == 0) {
    open_favorites_screen(ui);
    return;
  }
  if (strcmp(entry->id, "recent") == 0) {
    open_recent_screen(ui);
    return;
  }
  ui->screen = SCREEN_ROMS;
  set_status(ui, "loading ROM list");
  if (!load_rom_entries(ui, entry->id)) {
    set_status(ui, "cannot load ROM list");
  } else if (ui->rom_count == UI_MAX_ROMS) {
    set_status(ui, "ROM list truncated at prototype limit");
  } else if (ui->rom_scan_background_started) {
    set_status(ui, "ROM list ready; refreshing scan");
  } else {
    set_status(ui, "ROM list ready");
  }
  if (ui_uses_graphic_mode(ui) && ui->rom_entry_screen == SCREEN_GALLERY &&
      ui->rom_count > 0) {
    ui->gallery_back_screen = SCREEN_TOP;
    ui->screen = SCREEN_GALLERY;
    ui->gallery_transition_active = 0;
    ui->gallery_pending_active = 0;
    set_status(ui, "Gallery ready");
  }
  reset_marquee(ui);
}

static void open_gallery_screen(struct ui_state *ui) {
  if (!ui || ui->rom_count == 0) {
    set_status(ui, "no ROM entries for Gallery");
    return;
  }
  ui->gallery_back_screen = ui->screen;
  ui->screen = SCREEN_GALLERY;
  ui->gallery_transition_active = 0;
  ui->gallery_pending_active = 0;
  set_status(ui, "Gallery ready");
  reset_marquee(ui);
}

static int update_settings_entries_after_save(struct ui_state *ui);

static int run_usb_disk_mode(struct ui_state *ui) {
  char script[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!ui) {
    return 0;
  }
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-usb-disk-mode")) {
    set_status(ui, "USB Disk Mode path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "USB Disk Mode helper missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "cd / && export PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos,
                     " PLUMOS_USB_DISK_MODE_EXPERIMENTAL=1"
                     " PLUMOS_USB_DISK_MODE_CONFIRM=UNMOUNT_SDCARD; exec ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos,
                     " enter >>/tmp/plumos-usb-disk-mode.ui.log 2>&1")) {
    set_status(ui, "USB Disk Mode command too long");
    return 0;
  }

  rc = system(cmd);
  ui->usb_disk_start_due_ms = 0;
  if (rc == -1) {
    open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK_SERVICE);
    select_setting_entry_by_id(ui, "network_usb_disk_mode");
    set_status(ui, "USB Disk Mode system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK_SERVICE);
    select_setting_entry_by_id(ui, "network_usb_disk_mode");
    set_status(ui, "USB Disk Mode finished");
    return 1;
  }
  open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK_SERVICE);
  select_setting_entry_by_id(ui, "network_usb_disk_mode");
  set_status(ui, "USB Disk Mode returned non-zero");
  return 0;
}

static int run_network_wifi_control(struct ui_state *ui, int enable) {
  char script[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!ui) {
    return 0;
  }
  if (enable) {
    set_status(ui, "Use Connect Wi-Fi; Network Recovery is disabled");
    return 1;
  }
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-network-control")) {
    set_status(ui, "network control path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "network control script missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, " --wifi off")) {
    set_status(ui, "network control command too long");
    return 0;
  }

  rc = system(cmd);
  if (rc == -1) {
    set_status(ui, "network control system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    update_settings_entries_after_save(ui);
    set_status(ui, "Wi-Fi runtime off");
    return 1;
  }
  set_status(ui, "network control returned non-zero");
  return 0;
}

static int run_network_service_control(struct ui_state *ui, const char *service,
                                       int enable) {
  char script[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  char status_message[128];
  size_t pos = 0;
  int rc;

  if (!ui || !service || !service[0]) {
    return 0;
  }
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-network-services")) {
    set_status(ui, "network services path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "network services script missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, enable ? " start " : " stop ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, service) ||
      !append_string(cmd, sizeof(cmd), &pos, " >/dev/null 2>&1")) {
    set_status(ui, "network service command too long");
    return 0;
  }

  rc = system(cmd);
  update_settings_entries_after_save(ui);
  if (rc == -1) {
    set_status(ui, "network service system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    snprintf(status_message, sizeof(status_message), "%s %s",
             service, enable ? "enabled" : "disabled");
    set_status(ui, status_message);
    return 1;
  }
  snprintf(status_message, sizeof(status_message), "%s %s failed",
           service, enable ? "start" : "stop");
  set_status(ui, status_message);
  return 0;
}

static void stop_ntp_for_manual_time(struct ui_state *ui) {
  char script[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;

  if (!ui ||
      !join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-stock-services") ||
      !file_exists(script)) {
    return;
  }
  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, " ntp-stop >/dev/null 2>&1")) {
    return;
  }
  system(cmd);
}

static int apply_manual_system_time(struct ui_state *ui) {
  struct tm local_tm;
  struct tm check_tm;
  time_t epoch;
  struct timeval tv;
  char value[64];

  if (!ui) {
    return 0;
  }
  if (!ui->manual_time_initialized) {
    init_manual_time_from_current(ui);
  }
  clamp_manual_time_fields(ui);
  apply_system_timezone_runtime(ui, ui->device.timezone, NULL, 0);

  memset(&local_tm, 0, sizeof(local_tm));
  local_tm.tm_year = (int)ui->manual_time_year - 1900;
  local_tm.tm_mon = (int)ui->manual_time_month - 1;
  local_tm.tm_mday = (int)ui->manual_time_day;
  local_tm.tm_hour = (int)ui->manual_time_hour;
  local_tm.tm_min = (int)ui->manual_time_minute;
  local_tm.tm_sec = 0;
  local_tm.tm_isdst = -1;
  epoch = mktime(&local_tm);
  if (epoch == (time_t)-1 || !localtime_r(&epoch, &check_tm) ||
      check_tm.tm_year != local_tm.tm_year ||
      check_tm.tm_mon != local_tm.tm_mon ||
      check_tm.tm_mday != local_tm.tm_mday ||
      check_tm.tm_hour != local_tm.tm_hour ||
      check_tm.tm_min != local_tm.tm_min) {
    set_status(ui, "manual time is invalid for this timezone");
    return 0;
  }

  stop_ntp_for_manual_time(ui);
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  if (settimeofday(&tv, NULL) != 0) {
    snprintf(ui->status, sizeof(ui->status), "manual time failed: %s",
             strerror(errno));
    return 0;
  }
  apply_system_timezone_runtime(ui, ui->device.timezone, NULL, 0);
  ui->manual_time_initialized = 0;
  update_settings_entries_after_save(ui);
  format_current_time_local(value, sizeof(value));
  snprintf(ui->status, sizeof(ui->status), "manual time set: %s; NTP stopped", value);
  return 1;
}

static void wifi_clear_result(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  ui->wifi_result_title[0] = '\0';
  ui->wifi_result_ip[0] = '\0';
  ui->wifi_result_gateway[0] = '\0';
  ui->wifi_result_gateway_ping[0] = '\0';
  ui->wifi_result_stage[0] = '\0';
  ui->wifi_result_success = 0;
}

static void open_wifi_connect_screen(struct ui_state *ui) {
  if (!ui) {
    return;
  }
  ui->wifi_back_screen = ui->screen;
  ui->screen = SCREEN_WIFI_CONNECT;
  ui->wifi_stage = WIFI_CONNECT_SELECT;
  ui->wifi_key_row = 0;
  ui->wifi_key_col = 0;
  ui->wifi_key_shift = 0;
  ui->wifi_password[0] = '\0';
  wifi_clear_result(ui);
  set_status(ui, "Scanning Wi-Fi SSIDs");
  render_ui(ui);
  wifi_scan_networks(ui);
}

static void wifi_back_to_network_settings(struct ui_state *ui, const char *status) {
  open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK);
  select_setting_entry_by_id(ui, "network_connect_wifi");
  set_status(ui, status ? status : "back to Network Settings");
}

static int wifi_selected_network_is_open(const struct ui_state *ui) {
  const struct wifi_network_entry *entry;

  if (!ui || ui->wifi_count == 0 || ui->wifi_cursor >= ui->wifi_count) {
    return 0;
  }
  entry = &ui->wifi_networks[ui->wifi_cursor];
  return strcmp(entry->security, "open") == 0;
}

static int wifi_write_connect_tempfile(struct ui_state *ui, char *path, size_t path_size) {
  char template_path[] = "/tmp/plumos-wifi-connect.XXXXXX";
  int fd;
  const char *ssid;

  if (!ui || !path || path_size == 0 || ui->wifi_count == 0 ||
      ui->wifi_cursor >= ui->wifi_count) {
    return 0;
  }
  ssid = ui->wifi_networks[ui->wifi_cursor].ssid;
  if (string_contains_line_break(ssid) || string_contains_line_break(ui->wifi_password)) {
    set_status(ui, "SSID/password contains unsupported newline");
    return 0;
  }
  fd = mkstemp(template_path);
  if (fd < 0) {
    set_status(ui, "cannot create Wi-Fi temp file");
    return 0;
  }
  if (!write_all_string(fd, ssid) ||
      !write_all_string(fd, "\n") ||
      !write_all_string(fd, ui->wifi_password) ||
      !write_all_string(fd, "\n")) {
    close(fd);
    unlink(template_path);
    set_status(ui, "cannot write Wi-Fi temp file");
    return 0;
  }
  if (close(fd) != 0) {
    unlink(template_path);
    set_status(ui, "cannot close Wi-Fi temp file");
    return 0;
  }
  if (!copy_string(path, path_size, template_path)) {
    unlink(template_path);
    set_status(ui, "Wi-Fi temp path too long");
    return 0;
  }
  return 1;
}

static void wifi_parse_connect_output_line(struct ui_state *ui, const char *line) {
  const char *value;

  if (!ui || !line) {
    return;
  }
  if (strncmp(line, "result=", 7) == 0) {
    value = line + 7;
    if (strcmp(value, "connected") == 0) {
      ui->wifi_result_success = 1;
      copy_string(ui->wifi_result_title, sizeof(ui->wifi_result_title), "Connected");
    } else {
      ui->wifi_result_success = 0;
      copy_string(ui->wifi_result_title, sizeof(ui->wifi_result_title), "Connection Failed");
    }
  } else if (strncmp(line, "ip=", 3) == 0) {
    copy_truncated_string(ui->wifi_result_ip, sizeof(ui->wifi_result_ip), line + 3);
  } else if (strncmp(line, "gateway=", 8) == 0) {
    copy_truncated_string(ui->wifi_result_gateway, sizeof(ui->wifi_result_gateway),
                          line + 8);
  } else if (strncmp(line, "gateway_ping=", 13) == 0) {
    copy_truncated_string(ui->wifi_result_gateway_ping,
                          sizeof(ui->wifi_result_gateway_ping), line + 13);
  } else if (strncmp(line, "stage=", 6) == 0) {
    copy_prefixed_truncated_string(ui->wifi_result_stage, sizeof(ui->wifi_result_stage),
                                   "Failed at ", line + 6);
  }
}

static int run_wifi_connect_selected(struct ui_state *ui) {
  char script[PATH_MAX];
  char temp_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  char line[512];
  FILE *pipe;
  size_t pos = 0;
  int rc;
  size_t password_len;

  if (!ui || ui->wifi_count == 0 || ui->wifi_cursor >= ui->wifi_count) {
    set_status(ui, "No SSID selected");
    return 0;
  }
  password_len = strlen(ui->wifi_password);
  if (!wifi_selected_network_is_open(ui) &&
      (password_len < 8 || password_len > UI_WIFI_PASSWORD_MAX)) {
    set_status(ui, "Password must be 8..64 chars");
    return 0;
  }
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-network-control")) {
    set_status(ui, "network control path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "network control script missing");
    return 0;
  }
  if (!wifi_write_connect_tempfile(ui, temp_path, sizeof(temp_path))) {
    return 0;
  }

  wifi_clear_result(ui);
  copy_string(ui->wifi_result_title, sizeof(ui->wifi_result_title), "Connecting...");
  copy_string(ui->wifi_result_stage, sizeof(ui->wifi_result_stage), "Waiting for IP address.");
  ui->wifi_stage = WIFI_CONNECT_RESULT;
  set_status(ui, "Connecting Wi-Fi");
  render_ui(ui);

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, " --connect-file ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, temp_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>/dev/null")) {
    unlink(temp_path);
    set_status(ui, "Wi-Fi connect command too long");
    return 0;
  }

  pipe = popen(cmd, "r");
  if (!pipe) {
    unlink(temp_path);
    set_status(ui, "Wi-Fi connect failed to start");
    return 0;
  }
  while (fgets(line, sizeof(line), pipe)) {
    trim_line_end(line);
    wifi_parse_connect_output_line(ui, line);
  }
  rc = pclose(pipe);
  unlink(temp_path);
  settle_input_after_child(ui);

  if (ui->wifi_result_success) {
    if (!ui->wifi_result_stage[0]) {
      snprintf(ui->wifi_result_stage, sizeof(ui->wifi_result_stage),
               "Gateway ping: %s",
               ui->wifi_result_gateway_ping[0] ? ui->wifi_result_gateway_ping : "skipped");
    }
    snprintf(ui->status, sizeof(ui->status), "Wi-Fi connected IP=%s",
             ui->wifi_result_ip[0] ? ui->wifi_result_ip : "-");
    return 1;
  }
  if (!ui->wifi_result_title[0]) {
    copy_string(ui->wifi_result_title, sizeof(ui->wifi_result_title), "Connection Failed");
  }
  if (!ui->wifi_result_stage[0]) {
    copy_string(ui->wifi_result_stage, sizeof(ui->wifi_result_stage),
                "No connection result returned.");
  }
  if (rc == -1 || !(WIFEXITED(rc) && WEXITSTATUS(rc) == 0)) {
    set_status(ui, "Wi-Fi connection failed");
  } else {
    set_status(ui, "Wi-Fi connection did not complete");
  }
  return 0;
}

static int run_safe_shutdown(struct ui_state *ui, const char *action, int poweroff) {
  char script[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  const char *dry_run;
  const char *sleep_backend;
  const char *sleep_wakeup_sec;
  const char *power_backend;
  size_t pos = 0;
  int rc;

  if (!action || (strcmp(action, "shutdown") != 0 && strcmp(action, "sleep") != 0)) {
    set_status(ui, "safe action is invalid");
    return 0;
  }
  if (!join_path(script, sizeof(script), ui->plumos_root, "bin/plumos-safe-shutdown") ||
      !join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-safe-shutdown.log")) {
    set_status(ui, "safe shutdown path too long");
    return 0;
  }
  if (!file_exists(script)) {
    set_status(ui, "safe shutdown script missing");
    return 0;
  }

  dry_run = getenv("PLUMOS_CONTROLLER_SAFE_DRY_RUN");
  sleep_backend = getenv("PLUMOS_CONTROLLER_SAFE_SLEEP_BACKEND");
  sleep_wakeup_sec = getenv("PLUMOS_CONTROLLER_SAFE_SLEEP_WAKEUP_SEC");
  power_backend = getenv("PLUMOS_CONTROLLER_SAFE_POWER_BACKEND");
  if (!sleep_backend || !sleep_backend[0]) {
    sleep_backend = "mem";
  }
  if (!sleep_wakeup_sec) {
    sleep_wakeup_sec = "";
  }
  if (!power_backend || !power_backend[0]) {
    power_backend = "auto";
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, script) ||
      !append_string(cmd, sizeof(cmd), &pos, " --") ||
      !append_string(cmd, sizeof(cmd), &pos, action)) {
    set_status(ui, "safe shutdown command too long");
    return 0;
  }
  if (strcmp(action, "sleep") == 0) {
    if (!append_string(cmd, sizeof(cmd), &pos, " --sleep-backend ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, sleep_backend) ||
        !append_string(cmd, sizeof(cmd), &pos, " --no-poweroff")) {
      set_status(ui, "safe shutdown command too long");
      return 0;
    }
    if (sleep_wakeup_sec[0] &&
        (!append_string(cmd, sizeof(cmd), &pos, " --wakeup-sec ") ||
         !append_shell_quoted(cmd, sizeof(cmd), &pos, sleep_wakeup_sec))) {
      set_status(ui, "safe shutdown command too long");
      return 0;
    }
  } else if (poweroff) {
    if (!append_string(cmd, sizeof(cmd), &pos, " --poweroff --power-backend ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, power_backend)) {
      set_status(ui, "safe shutdown command too long");
      return 0;
    }
  } else if (!append_string(cmd, sizeof(cmd), &pos, " --no-poweroff")) {
    set_status(ui, "safe shutdown command too long");
    return 0;
  }
  if (dry_run && dry_run[0] && strcmp(dry_run, "0") != 0 &&
      strcmp(dry_run, "false") != 0 && strcmp(dry_run, "off") != 0 &&
      !append_string(cmd, sizeof(cmd), &pos, " --dry-run")) {
    set_status(ui, "safe shutdown command too long");
    return 0;
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " >>") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    set_status(ui, "safe shutdown command too long");
    return 0;
  }

  snprintf(ui->status, sizeof(ui->status), "safe %s running", action);
  render_ui(ui);
  rc = system(cmd);
  if (rc == -1) {
    set_status(ui, "safe shutdown system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    if (strcmp(action, "sleep") == 0) {
      snprintf(ui->status, sizeof(ui->status), "safe sleep complete backend=%s", sleep_backend);
    } else {
      snprintf(ui->status, sizeof(ui->status), "safe shutdown complete%s",
               poweroff ? " poweroff" : " (no poweroff)");
    }
    return 1;
  }
  set_status(ui, "safe shutdown returned non-zero; see frontend-safe-shutdown.log");
  return 0;
}

static int launch_rom_entry(struct ui_state *ui, const struct rom_entry *entry) {
  char text_ui[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  const char *system_id;
  size_t pos = 0;
  int rc;

  if (!entry || !entry->relative_path[0]) {
    set_status(ui, "launch target is empty");
    return 0;
  }
  system_id = entry->system_id[0] ? entry->system_id : ui->current_system_id;
  if (!valid_system_id(system_id)) {
    set_status(ui, "launch system id is invalid");
    return 0;
  }
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui") ||
      !join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-launch.log")) {
    set_status(ui, "launch path too long");
    return 0;
  }
  if (!file_exists(text_ui)) {
    set_status(ui, "plumos-text-ui missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, " launch ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, system_id) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->relative_path)) {
    set_status(ui, "launch command too long");
    return 0;
  }
  if (ui->screen == SCREEN_RECENT && entry->launch_profile[0]) {
    if (!append_string(cmd, sizeof(cmd), &pos, " --profile ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->launch_profile)) {
      set_status(ui, "launch command too long");
      return 0;
    }
  }
  if (!append_string(cmd, sizeof(cmd), &pos, " --execute --no-scan >>") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    set_status(ui, "launch command too long");
    return 0;
  }

  snprintf(ui->status, sizeof(ui->status), "launching %.32s / %.120s", system_id,
           entry->relative_path);
  render_ui(ui);
  shutdown_ui_renderer(ui);
  rc = system(cmd);
  if (ui->renderer_mali && !init_ui_renderer(ui)) {
    ui->renderer_mali = 0;
  }
  settle_input_after_child(ui);
  load_device_settings(ui);
  if (ui->screen == SCREEN_RECENT) {
    load_recent_entries(ui);
  }
  if (rc == -1) {
    set_status(ui, "launch system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    set_status(ui, "launch finished; FE resumed");
    return 1;
  }
  set_status(ui, "launch returned non-zero; see frontend-launch.log");
  return 0;
}

static int string_is_unsigned_int(const char *value) {
  if (!value || !value[0]) {
    return 0;
  }
  while (*value) {
    if (!isdigit((unsigned char)*value)) {
      return 0;
    }
    value++;
  }
  return 1;
}

static const char *scraping_limit_env(const char *name) {
  const char *value = getenv(name);
  return string_is_unsigned_int(value) ? value : NULL;
}

static int append_scraping_target_list(char *cmd, size_t cmd_size, size_t *pos,
                                       const struct ui_state *ui) {
  size_t i;

  if (!cmd || !pos || !ui || ui->scraping_choice_count == 0) {
    return 0;
  }
  if (ui->scraping_choice_cursor > 0 &&
      ui->scraping_choice_cursor <= ui->scraping_choice_count) {
    return append_string(cmd, cmd_size, pos, " ") &&
           append_shell_quoted(cmd, cmd_size, pos,
                               ui->scraping_choices[ui->scraping_choice_cursor - 1].id);
  }
  for (i = 0; i < ui->scraping_choice_count; i++) {
    if (!append_string(cmd, cmd_size, pos, " ") ||
        !append_shell_quoted(cmd, cmd_size, pos, ui->scraping_choices[i].id)) {
      return 0;
    }
  }
  return 1;
}

static int append_scraping_runner_loop(char *cmd, size_t cmd_size, size_t *pos,
                                       const struct ui_state *ui,
                                       const char *scraper,
                                       const char *path_value,
                                       int fetch_mode,
                                       const char *scraper_kind,
                                       int replace_existing,
                                       const char *limit,
                                       const char *fetch_timeout,
                                       const char *fetch_retry) {
  if (!append_string(cmd, cmd_size, pos, "; for sys in") ||
      !append_scraping_target_list(cmd, cmd_size, pos, ui)) {
    return 0;
  }
  if (fetch_mode) {
    if (!append_string(cmd, cmd_size, pos, "; do PLUMOS_THUMBNAIL_PROGRESS=1")) {
      return 0;
    }
    if (fetch_timeout &&
        (!append_string(cmd, cmd_size, pos, " PLUMOS_THUMBNAIL_FETCH_TIMEOUT=") ||
         !append_shell_quoted(cmd, cmd_size, pos, fetch_timeout))) {
      return 0;
    }
    if (fetch_retry &&
        (!append_string(cmd, cmd_size, pos, " PLUMOS_THUMBNAIL_FETCH_RETRY=") ||
         !append_shell_quoted(cmd, cmd_size, pos, fetch_retry))) {
      return 0;
    }
    if (!append_string(cmd, cmd_size, pos, " PLUMOS_SDCARD_ROOT=")) {
      return 0;
    }
  } else if (!append_string(cmd, cmd_size, pos,
                            "; do progress_i=$((progress_i + 1)); printf 'progress\\tplan\\t%s\\t%s\\t%s\\t0\\t0\\t0\\n' \"$sys\" \"$progress_i\" \"$progress_total\"; PLUMOS_SDCARD_ROOT=")) {
    return 0;
  }
  if (!append_shell_quoted(cmd, cmd_size, pos, ui->sdcard_root) ||
      !append_string(cmd, cmd_size, pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, cmd_size, pos, ui->plumos_root) ||
      !append_string(cmd, cmd_size, pos, " PATH=") ||
      !append_shell_quoted(cmd, cmd_size, pos, path_value) ||
      !append_string(cmd, cmd_size, pos, " ") ||
      !append_shell_quoted(cmd, cmd_size, pos, scraper)) {
    return 0;
  }
  if (fetch_mode && !append_string(cmd, cmd_size, pos, " --fetch")) {
    return 0;
  }
  if (scraper_kind && scraper_kind[0] &&
      (!append_string(cmd, cmd_size, pos, " --kind ") ||
       !append_shell_quoted(cmd, cmd_size, pos, scraper_kind))) {
    return 0;
  }
  if (replace_existing &&
      !append_string(cmd, cmd_size, pos, " --replace-existing")) {
    return 0;
  }
  if (!append_string(cmd, cmd_size, pos, " --system \"$sys\"")) {
    return 0;
  }
  if (limit && (!append_string(cmd, cmd_size, pos, " --limit ") ||
                !append_shell_quoted(cmd, cmd_size, pos, limit))) {
    return 0;
  }
  return append_string(cmd, cmd_size, pos,
                       "; step_rc=$?; [ \"$step_rc\" -eq 0 ] || app_rc=\"$step_rc\"; done");
}

static int run_command_with_live_log(struct ui_state *ui, const char *cmd,
                                     const char *latest_path, const char *log_path) {
  FILE *pipe;
  FILE *latest;
  FILE *log;
  char line[UI_RENDER_LINE_MAX];
  char trimmed[UI_RENDER_LINE_MAX];
  int rc;

  if (!ui || !cmd || !latest_path || !log_path) {
    return -1;
  }
  latest = fopen(latest_path, "wb");
  if (!latest) {
    set_status(ui, "cannot write latest result log");
    return -1;
  }
  log = fopen(log_path, "ab");
  if (!log) {
    fclose(latest);
    set_status(ui, "cannot write app result log");
    return -1;
  }
  pipe = popen(cmd, "r");
  if (!pipe) {
    fclose(log);
    fclose(latest);
    set_status(ui, "cannot start scraping command");
    return -1;
  }

  while (fgets(line, sizeof(line), pipe)) {
    fputs(line, latest);
    fflush(latest);
    fputs(line, log);
    fflush(log);
    copy_truncated_string(trimmed, sizeof(trimmed), line);
    trim_line_end(trimmed);
    if (update_thumbnail_running_progress_from_log_line(ui, trimmed)) {
      render_ui(ui);
    }
  }
  rc = pclose(pipe);
  fclose(log);
  fclose(latest);
  return rc;
}

static int run_scraping_action(struct ui_state *ui) {
  char scraper[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char latest_path[PATH_MAX];
  char path_value[PATH_MAX * 2];
  char cmd[UI_COMMAND_MAX];
  const char *plan_limit;
  const char *fetch_limit;
  const char *fetch_timeout;
  const char *fetch_retry;
  const struct scraping_kind_choice *kind;
  size_t pos = 0;
  size_t path_pos = 0;
  size_t target_count;
  int rc;

  if (!ui || ui->scraping_choice_count == 0) {
    set_status(ui, "no scraping target");
    return 0;
  }
  if (!join_path(scraper, sizeof(scraper), ui->plumos_root,
                 "bin/plumos-thumbnail-scraper") ||
      !join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-apps.log") ||
      !join_path(latest_path, sizeof(latest_path), log_dir,
                 "frontend-apps-latest.log")) {
    set_status(ui, "scraping path too long");
    return 0;
  }
  if (!file_exists(scraper)) {
    set_status(ui, "scraper is missing");
    return 0;
  }
  path_value[0] = '\0';
  if (!append_string(path_value, sizeof(path_value), &path_pos, ui->plumos_root) ||
      !append_string(path_value, sizeof(path_value), &path_pos, "/bin:") ||
      !append_string(path_value, sizeof(path_value), &path_pos, ui->plumos_root) ||
      !append_string(path_value, sizeof(path_value), &path_pos,
                     "/gnu/bin:/usr/sbin:/usr/bin:/sbin:/bin")) {
    set_status(ui, "scraping PATH too long");
    return 0;
  }
  plan_limit = scraping_limit_env("PLUMOS_SCRAPING_PLAN_LIMIT");
  fetch_limit = scraping_limit_env("PLUMOS_SCRAPING_FETCH_LIMIT");
  fetch_timeout = scraping_limit_env("PLUMOS_SCRAPING_FETCH_TIMEOUT");
  fetch_retry = scraping_limit_env("PLUMOS_SCRAPING_FETCH_RETRY");
  if (!fetch_timeout) {
    fetch_timeout = "12";
  }
  if (!fetch_retry) {
    fetch_retry = "0";
  }
  kind = scraping_selected_kind(ui);
  target_count = ui->scraping_choice_cursor == 0 ? ui->scraping_choice_count : (size_t)1;

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd),
                     &pos, "; { printf 'app_start\\t%s\\n' ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, "thumbnail-scraping") ||
      !append_string(cmd, sizeof(cmd),
                     &pos, "; printf 'scraping_options\\timage=%s\\tkind=%s\\texisting=%s\\n' ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, kind->display_name) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, kind->scraper_kind) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, scraping_existing_label(ui)) ||
      !append_string(cmd, sizeof(cmd), &pos, "; app_rc=0; progress_i=0; progress_total=") ||
      !append_size_t(cmd, sizeof(cmd), &pos, target_count) ||
      !append_scraping_runner_loop(cmd, sizeof(cmd), &pos, ui, scraper,
                                   path_value, 0, kind->scraper_kind,
                                   ui->scraping_replace_existing,
                                   plan_limit, NULL, NULL) ||
      !append_scraping_runner_loop(cmd, sizeof(cmd), &pos, ui, scraper,
                                   path_value, 1, kind->scraper_kind,
                                   ui->scraping_replace_existing,
                                   fetch_limit, fetch_timeout, fetch_retry) ||
      !append_string(cmd, sizeof(cmd),
                     &pos, "; printf 'app_finish\\t%s\\trc=%s\\n' ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, "thumbnail-scraping") ||
      !append_string(cmd, sizeof(cmd), &pos,
                     " \"$app_rc\"; [ \"$app_rc\" -eq 0 ]; } 2>&1")) {
    set_status(ui, "scraping command too long");
    return 0;
  }

  copy_string(ui->thumbnail_running_title, sizeof(ui->thumbnail_running_title),
              "Scraping");
  reset_thumbnail_running_progress(ui);
  ui->screen = SCREEN_THUMBNAIL_RUNNING;
  snprintf(ui->status, sizeof(ui->status), "running Scraping %s",
           kind->display_name);
  render_ui(ui);
  mkdir(log_dir, 0755);
  rc = run_command_with_live_log(ui, cmd, latest_path, log_path);
  settle_input_after_child(ui);
  ui->screen = SCREEN_THUMBNAIL_RESULTS;
  ui->thumbnail_result_cursor = 0;
  load_thumbnail_results(ui);
  if (system_command_succeeded(rc)) {
    set_status(ui, "Scraping finished");
    return 1;
  }
  if (rc == -1) {
    set_status(ui, "scraping system call failed");
    return 0;
  }
  set_status(ui, "Scraping returned non-zero");
  return 0;
}

static int run_menu_shell_action(struct ui_state *ui, const struct menu_entry *entry) {
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char latest_path[PATH_MAX];
  char lock_dir[PATH_MAX];
  char path_value[PATH_MAX * 2];
  char cmd[UI_COMMAND_MAX];
  const char *shell_cmd;
  size_t pos = 0;
  size_t path_pos = 0;
  int rc;

  if (!ui || !entry || strncmp(entry->action, "shell:", 6) != 0) {
    set_status(ui, "app action is not a shell command");
    return 0;
  }
  shell_cmd = entry->action + 6;
  if (!shell_cmd[0]) {
    set_status(ui, "app shell command is empty");
    return 0;
  }
  if (!join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-apps.log")) {
    set_status(ui, "app log path too long");
    return 0;
  }
  if (entry->show_results &&
      !join_path(latest_path, sizeof(latest_path), log_dir, "frontend-apps-latest.log")) {
    set_status(ui, "app latest log path too long");
    return 0;
  }
  path_value[0] = '\0';
  if (!append_string(path_value, sizeof(path_value), &path_pos, ui->plumos_root) ||
      !append_string(path_value, sizeof(path_value), &path_pos, "/bin:") ||
      !append_string(path_value, sizeof(path_value), &path_pos, ui->plumos_root) ||
      !append_string(path_value, sizeof(path_value), &path_pos,
                     "/gnu/bin:/usr/sbin:/usr/bin:/sbin:/bin")) {
    set_status(ui, "app PATH too long");
    return 0;
  }
  lock_dir[0] = '\0';
  if (entry->background) {
    if (!valid_system_id(entry->id)) {
      set_status(ui, "background app id is invalid");
      return 0;
    }
    snprintf(lock_dir, sizeof(lock_dir), "/tmp/plumos-app-%s.lock", entry->id);
  }

  cmd[0] = '\0';
  if (entry->show_results) {
    if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
        !append_string(cmd, sizeof(cmd), &pos, "; { printf 'app_start\\t%s\\n' ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->id) ||
        !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PATH=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, path_value) ||
        !append_string(cmd, sizeof(cmd), &pos, " /bin/sh -c ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, shell_cmd) ||
        !append_string(cmd, sizeof(cmd), &pos,
                       "; app_rc=$?; printf 'app_finish\\t%s\\trc=%s\\n' ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->id) ||
        !append_string(cmd, sizeof(cmd), &pos,
                       " \"$app_rc\"; [ \"$app_rc\" -eq 0 ]; } >") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, latest_path) ||
        !append_string(cmd, sizeof(cmd), &pos, " 2>&1; rc=$?; cat ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, latest_path) ||
        !append_string(cmd, sizeof(cmd), &pos, " >>") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
        !append_string(cmd, sizeof(cmd), &pos, "; exit \"$rc\"")) {
      set_status(ui, "app command too long");
      return 0;
    }
  } else if (entry->background) {
    if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
        !append_string(cmd, sizeof(cmd), &pos, "; trap '' HUP; if mkdir ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, lock_dir) ||
        !append_string(cmd, sizeof(cmd), &pos, " 2>/dev/null; then ( trap 'rm -rf ") ||
        !append_string(cmd, sizeof(cmd), &pos, lock_dir) ||
        !append_string(cmd, sizeof(cmd), &pos, "' EXIT INT TERM; printf 'app_start\\t%s\\n' ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->id) ||
        !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PATH=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, path_value) ||
        !append_string(cmd, sizeof(cmd), &pos, " /bin/sh -c ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, shell_cmd) ||
        !append_string(cmd, sizeof(cmd), &pos,
                       "; rc=$?; printf 'app_finish\\t%s\\trc=%s\\n' ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->id) ||
        !append_string(cmd, sizeof(cmd), &pos, " \"$rc\"; exit \"$rc\" ) >>") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
        !append_string(cmd, sizeof(cmd), &pos,
                       " 2>&1 </dev/null & else printf 'app_already_running\\t%s\\n' ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, entry->id) ||
        !append_string(cmd, sizeof(cmd), &pos, " >>") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
        !append_string(cmd, sizeof(cmd), &pos, "; exit 3; fi")) {
      set_status(ui, "app command too long");
      return 0;
    }
  } else {
    if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
        !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
        !append_string(cmd, sizeof(cmd), &pos, " PATH=") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, path_value) ||
        !append_string(cmd, sizeof(cmd), &pos, " /bin/sh -c ") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, shell_cmd) ||
        !append_string(cmd, sizeof(cmd), &pos, " >>") ||
        !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
        !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
      set_status(ui, "app command too long");
      return 0;
    }
  }

  if (entry->show_results) {
    copy_string(ui->thumbnail_running_title, sizeof(ui->thumbnail_running_title),
                entry->display_name);
    ui->screen = SCREEN_THUMBNAIL_RUNNING;
    snprintf(ui->status, sizeof(ui->status), "running %.80s", entry->display_name);
  } else {
    snprintf(ui->status, sizeof(ui->status), "%s %.80s",
             entry->background ? "starting" : "running", entry->display_name);
  }
  render_ui(ui);
  shutdown_ui_renderer(ui);
  rc = system(cmd);
  if (ui->renderer_mali && !init_ui_renderer(ui)) {
    ui->renderer_mali = 0;
  }
  settle_input_after_child(ui);
  if (entry->show_results) {
    ui->screen = SCREEN_THUMBNAIL_RESULTS;
    ui->thumbnail_result_cursor = 0;
    load_thumbnail_results(ui);
    if (system_command_succeeded(rc)) {
      snprintf(ui->status, sizeof(ui->status), "%.80s finished", entry->display_name);
      return 1;
    }
    if (rc == -1) {
      set_status(ui, "app system call failed");
      return 0;
    }
    set_status(ui, "app returned non-zero; see latest result");
    return 0;
  }
  if (system_command_succeeded(rc)) {
    snprintf(ui->status, sizeof(ui->status), "%.80s %s", entry->display_name,
             entry->background ? "started; see frontend-apps.log" : "finished");
    return 1;
  }
  if (rc == -1) {
    set_status(ui, "app system call failed");
    return 0;
  }
  if (entry->background && WIFEXITED(rc) && WEXITSTATUS(rc) == 3) {
    snprintf(ui->status, sizeof(ui->status), "%.80s already running", entry->display_name);
    return 0;
  }
  set_status(ui, "app returned non-zero; see frontend-apps.log");
  return 0;
}

static int bool_from_setting_value(const char *value) {
  return setting_value_is_true(value);
}

static int brightness_test_tiles_enabled(void) {
  const char *value = getenv("PLUMOS_CONTROLLER_BRIGHTNESS_TEST");

  return value && setting_value_is_true(value);
}

static void settings_start_arrow_blink(struct ui_state *ui, int direction) {
  if (!ui || direction == 0) {
    return;
  }
  ui->settings_blink_cursor = ui->settings_cursor;
  ui->settings_blink_direction = direction < 0 ? -1 : 1;
  ui->settings_blink_until_ms = current_time_ms() + 240;
}

static long clamp_long(long value, long min_value, long max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static int update_settings_entries_after_save(struct ui_state *ui) {
  size_t cursor;

  if (!ui) {
    return 0;
  }
  cursor = ui->settings_cursor;
  if (!load_settings_entries(ui)) {
    set_status(ui, "settings saved; reload failed");
    return 0;
  }
  if (ui->setting_count == 0) {
    ui->settings_cursor = 0;
  } else if (cursor >= ui->setting_count) {
    ui->settings_cursor = ui->setting_count - 1;
  } else {
    ui->settings_cursor = cursor;
  }
  return 1;
}

static void refresh_top_entries_preserve_cursor(struct ui_state *ui) {
  char selected_id[64] = "";
  size_t i;

  if (!ui) {
    return;
  }
  if (ui->top_count > 0 && ui->top_cursor < ui->top_count) {
    copy_string(selected_id, sizeof(selected_id), ui->top_entries[ui->top_cursor].id);
  }
  if (!load_top_entries(ui)) {
    return;
  }
  if (!selected_id[0]) {
    return;
  }
  for (i = 0; i < ui->top_count; i++) {
    if (strcmp(ui->top_entries[i].id, selected_id) == 0) {
      ui->top_cursor = i;
      return;
    }
  }
}

static int refresh_current_rom_entries_preserve_cursor(struct ui_state *ui) {
  char selected_relative_path[UI_PATH_MAX] = "";
  char system_id[64] = "";
  size_t i;

  if (!ui || !ui->current_system_id[0] ||
      strcmp(ui->current_system_id, "favorites") == 0 ||
      strcmp(ui->current_system_id, "recent") == 0) {
    return 0;
  }
  copy_string(system_id, sizeof(system_id), ui->current_system_id);
  if (ui->rom_count > 0 && ui->rom_cursor < ui->rom_count) {
    copy_string(selected_relative_path, sizeof(selected_relative_path),
                ui->rom_entries[ui->rom_cursor].relative_path);
  }
  if (!load_rom_entries(ui, system_id)) {
    return 0;
  }
  if (!selected_relative_path[0]) {
    reset_marquee(ui);
    return 1;
  }
  for (i = 0; i < ui->rom_count; i++) {
    if (strcmp(ui->rom_entries[i].relative_path, selected_relative_path) == 0) {
      ui->rom_cursor = i;
      break;
    }
  }
  reset_marquee(ui);
  return 1;
}

static int poll_rom_scan_refresh(struct ui_state *ui) {
  int status = 0;
  pid_t rc;
  char finished_system_id[64];
  int refresh_ok;

  if (!ui || ui->rom_scan_refresh_pid <= 0) {
    return 0;
  }
  rc = waitpid(ui->rom_scan_refresh_pid, &status, WNOHANG);
  if (rc == 0 || (rc < 0 && errno == EINTR)) {
    return 0;
  }
  copy_string(finished_system_id, sizeof(finished_system_id),
              ui->rom_scan_refresh_system_id);
  ui->rom_scan_refresh_pid = 0;
  if (rc < 0) {
    set_status(ui, "background ROM scan status lost");
    return 1;
  }
  if (!system_command_succeeded(status)) {
    if (ui->screen == SCREEN_ROMS &&
        strcmp(ui->current_system_id, finished_system_id) == 0) {
      set_status(ui, "background ROM scan failed");
      return 1;
    }
    return 0;
  }
  if (ui->screen != SCREEN_ROMS ||
      strcmp(ui->current_system_id, finished_system_id) != 0) {
    return 0;
  }

  ui->rom_scan_refresh_suppressed = 1;
  refresh_ok = refresh_current_rom_entries_preserve_cursor(ui);
  ui->rom_scan_refresh_suppressed = 0;
  set_status(ui, refresh_ok ? "ROM list refreshed" : "ROM scan done; reload failed");
  return 1;
}

static void refresh_runtime_after_setting_save(struct ui_state *ui, const char *id) {
  if (!ui || !id) {
    return;
  }
  if (strcmp(id, "show_empty_systems") == 0 ||
      strcmp(id, "show_favorites_on_top") == 0 ||
      strcmp(id, "show_recent_on_top") == 0 ||
      strcmp(id, "sort_systems") == 0) {
    refresh_top_entries_preserve_cursor(ui);
  }
  if (strcmp(id, "sort_roms") == 0) {
    refresh_current_rom_entries_preserve_cursor(ui);
  }
}

static int run_performance_text_ui_core_system(struct ui_state *ui, const char *extra_args) {
  char text_ui[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  size_t pos = 0;
  int rc;

  if (!ui || !performance_ensure_system(ui)) {
    set_status(ui, "no performance system selected");
    return 0;
  }
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui") ||
      !join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-performance.log")) {
    set_status(ui, "performance command path too long");
    return 0;
  }
  if (!file_exists(text_ui)) {
    set_status(ui, "plumos-text-ui missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, " core system ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->performance_system_id) ||
      !append_string(cmd, sizeof(cmd), &pos, extra_args ? extra_args : "") ||
      !append_string(cmd, sizeof(cmd), &pos, " >>") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    set_status(ui, "performance command too long");
    return 0;
  }

  rc = system(cmd);
  if (rc == -1) {
    set_status(ui, "performance command failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    return 1;
  }
  set_status(ui, "performance command returned non-zero");
  return 0;
}

static int save_performance_system_choice(struct ui_state *ui, int direction) {
  char selected[128];

  if (!ui || direction == 0) {
    return 0;
  }
  if (!performance_cycle_system(ui, direction)) {
    set_status(ui, "no performance system available");
    return 0;
  }
  copy_string(selected, sizeof(selected),
              ui->performance_system_name[0] ? ui->performance_system_name
                                             : ui->performance_system_id);
  update_settings_entries_after_save(ui);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "selected system=%s", selected);
  return 1;
}

static int save_performance_cpu_policy_choice(struct ui_state *ui, int direction) {
  const struct performance_cpu_preset *preset;
  char extra[128];
  int index;

  if (!ui || direction == 0) {
    return 0;
  }
  load_performance_core_state(ui);
  index = performance_cpu_preset_index(ui->performance_cpu_policy,
                                       ui->performance_cpu_freq_khz);
  index += direction > 0 ? 1 : -1;
  if (index < 0) {
    index = (int)PERFORMANCE_CPU_PRESET_COUNT - 1;
  } else if ((size_t)index >= PERFORMANCE_CPU_PRESET_COUNT) {
    index = 0;
  }
  preset = &PERFORMANCE_CPU_PRESETS[index];
  if (strcmp(preset->policy, "fixed") == 0) {
    snprintf(extra, sizeof(extra), " --cpu fixed --freq %ld", preset->freq_khz);
  } else {
    snprintf(extra, sizeof(extra), " --cpu %s", preset->policy);
  }
  if (!run_performance_text_ui_core_system(ui, extra)) {
    return 0;
  }
  update_settings_entries_after_save(ui);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "saved CPU freq=%s", preset->label);
  return 1;
}

static int save_performance_cpu_cores_choice(struct ui_state *ui, int direction) {
  size_t i;
  size_t index = 0;
  char extra[64];
  long cores;

  if (!ui || direction == 0) {
    return 0;
  }
  load_performance_core_state(ui);
  for (i = 0; i < PERFORMANCE_CPU_CORE_PRESET_COUNT; i++) {
    if (ui->performance_cpu_cores == PERFORMANCE_CPU_CORE_PRESETS[i]) {
      index = i;
      break;
    }
  }
  if (direction > 0) {
    index = (index + 1) % PERFORMANCE_CPU_CORE_PRESET_COUNT;
  } else if (index == 0) {
    index = PERFORMANCE_CPU_CORE_PRESET_COUNT - 1;
  } else {
    index--;
  }
  cores = PERFORMANCE_CPU_CORE_PRESETS[index];
  snprintf(extra, sizeof(extra), " --cores %ld", cores);
  if (!run_performance_text_ui_core_system(ui, extra)) {
    return 0;
  }
  update_settings_entries_after_save(ui);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "saved CPU Cores=%ld", cores);
  return 1;
}

static int save_performance_setting_choice(struct ui_state *ui, const char *id,
                                           int direction) {
  if (!id) {
    return 0;
  }
  if (strcmp(id, "performance_system") == 0) {
    return save_performance_system_choice(ui, direction);
  }
  if (strcmp(id, "performance_cpu_policy") == 0) {
    return save_performance_cpu_policy_choice(ui, direction);
  }
  if (strcmp(id, "performance_cpu_cores") == 0) {
    return save_performance_cpu_cores_choice(ui, direction);
  }
  return 0;
}

static int clear_performance_cpu_override(struct ui_state *ui) {
  if (!run_performance_text_ui_core_system(ui, " --clear-cpu")) {
    return 0;
  }
  update_settings_entries_after_save(ui);
  set_status(ui, "reset CPU defaults");
  return 1;
}

static int save_setting_bool(struct ui_state *ui, const char *id, int value) {
  struct frontend_settings settings;

  if (!load_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings read failed");
    return 0;
  }
  if (strcmp(id, "show_empty_systems") == 0) {
    settings.show_empty_systems = value ? 1 : 0;
  } else if (strcmp(id, "show_favorites_on_top") == 0) {
    settings.show_favorites_on_top = value ? 1 : 0;
  } else if (strcmp(id, "show_recent_on_top") == 0) {
    settings.show_recent_on_top = value ? 1 : 0;
  } else if (strcmp(id, "rom_scan_policy") == 0) {
    copy_string(settings.rom_scan_policy, sizeof(settings.rom_scan_policy),
                value ? "on_enter" : "manual");
  } else {
    set_status(ui, "setting is read-only");
    return 0;
  }
  if (!save_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings write failed");
    return 0;
  }
  update_settings_entries_after_save(ui);
  refresh_runtime_after_setting_save(ui, id);
  snprintf(ui->status, sizeof(ui->status), "saved %s=%s", id, value ? "true" : "false");
  return 1;
}

static int choice_index_from_value(const struct setting_choice *choices, size_t count,
                                   const char *value) {
  size_t i;

  for (i = 0; i < count; i++) {
    if (strcmp(value, choices[i].raw) == 0 || strcmp(value, choices[i].display) == 0) {
      return (int)i;
    }
  }
  return 0;
}

static int graphic_theme_choice_index(const struct graphic_theme_choice *choices,
                                      size_t count, const char *raw_value,
                                      const char *display_value) {
  size_t i;

  for (i = 0; i < count; i++) {
    if ((raw_value && strcmp(choices[i].raw, raw_value) == 0) ||
        (display_value && strcmp(choices[i].display, display_value) == 0) ||
        (display_value && strcmp(choices[i].raw, display_value) == 0)) {
      return (int)i;
    }
  }
  return 0;
}

static int save_graphic_theme_choice(struct ui_state *ui,
                                     const char *display_value, int direction) {
  struct frontend_settings settings;
  struct graphic_theme_choice choices[UI_GRAPHIC_THEME_CHOICE_MAX];
  size_t count;
  int index;

  if (direction == 0) {
    set_status(ui, "setting needs LEFT/RIGHT");
    return 0;
  }
  count = load_graphic_theme_choices(ui, choices,
                                     sizeof(choices) / sizeof(choices[0]));
  if (count == 0) {
    set_status(ui, "no Graphic themes found");
    return 0;
  }
  if (!load_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings read failed");
    return 0;
  }
  index = graphic_theme_choice_index(choices, count, settings.graphic_theme_id,
                                     display_value);
  index += direction > 0 ? 1 : -1;
  if (index < 0) {
    index = (int)count - 1;
  } else if ((size_t)index >= count) {
    index = 0;
  }
  copy_string(settings.graphic_theme_id, sizeof(settings.graphic_theme_id),
              choices[index].raw);
  copy_string(settings.theme_id, sizeof(settings.theme_id), choices[index].raw);
  settings.graphic_top_layout[0] = '\0';
  settings.graphic_transition[0] = '\0';
  settings.graphic_transition_axis[0] = '\0';
  settings.graphic_transition_easing[0] = '\0';
  settings.graphic_transition_ms = 0;
  if (!save_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings write failed");
    return 0;
  }
  ui->frontend_settings = settings;
  load_theme_state(ui, settings.graphic_theme_id);
  apply_theme_setting_overrides(&ui->theme, &settings);
  update_settings_entries_after_save(ui);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "saved Graphic Theme=%s",
           choices[index].display);
  return 1;
}

static int save_setting_choice(struct ui_state *ui, const char *id,
                               const char *display_value, int direction) {
  struct frontend_settings settings;
  const struct setting_choice *choices;
  size_t count = 0;
  int index;
  const char *raw;

  if (strcmp(id, "graphic_theme_id") == 0) {
    return save_graphic_theme_choice(ui, display_value, direction);
  }

  choices = setting_choices(id, &count);
  if (!choices || count == 0 || direction == 0) {
    set_status(ui, "setting is not a choice");
    return 0;
  }
  index = choice_index_from_value(choices, count, display_value);
  index += direction > 0 ? 1 : -1;
  if (index < 0) {
    index = (int)count - 1;
  } else if ((size_t)index >= count) {
    index = 0;
  }
  raw = choices[index].raw;

  if (strcmp(id, "system_language") == 0) {
    if (!save_system_config_string(ui, "language", raw)) {
      set_status(ui, "plumOS system config write failed");
      return 0;
    }
    update_settings_entries_after_save(ui);
    settings_start_arrow_blink(ui, direction);
    snprintf(ui->status, sizeof(ui->status), "saved %s=%s", id, choices[index].display);
    return 1;
  }

  if (strcmp(id, "system_timezone") == 0) {
    char runtime_status[128];
    if (!save_system_config_string(ui, "timezone", raw)) {
      set_status(ui, "plumOS system config write failed");
      return 0;
    }
    copy_string(ui->device.timezone, sizeof(ui->device.timezone), raw);
    apply_system_timezone_runtime(ui, raw, runtime_status, sizeof(runtime_status));
    ui->manual_time_initialized = 0;
    update_settings_entries_after_save(ui);
    settings_start_arrow_blink(ui, direction);
    snprintf(ui->status, sizeof(ui->status), "saved Timezone=%s; %s",
             choices[index].display, runtime_status);
    return 1;
  }

  if (!load_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings read failed");
    return 0;
  }
  if (strcmp(id, "ui_mode") == 0) {
    copy_string(settings.ui_mode, sizeof(settings.ui_mode), raw);
    copy_string(settings.top_mode, sizeof(settings.top_mode), raw);
    copy_string(settings.rom_mode, sizeof(settings.rom_mode), raw);
  } else if (strcmp(id, "boot_resume_mode") == 0) {
    copy_string(settings.boot_resume_mode, sizeof(settings.boot_resume_mode), raw);
  } else if (strcmp(id, "theme_top_layout") == 0) {
    copy_string(settings.graphic_top_layout, sizeof(settings.graphic_top_layout), raw);
  } else if (strcmp(id, "theme_transition") == 0) {
    copy_string(settings.graphic_transition, sizeof(settings.graphic_transition), raw);
  } else if (strcmp(id, "theme_transition_axis") == 0) {
    copy_string(settings.graphic_transition_axis,
                sizeof(settings.graphic_transition_axis), raw);
  } else if (strcmp(id, "theme_transition_easing") == 0) {
    copy_string(settings.graphic_transition_easing,
                sizeof(settings.graphic_transition_easing), raw);
  } else if (strcmp(id, "sort_systems") == 0) {
    copy_string(settings.sort_systems, sizeof(settings.sort_systems), raw);
  } else if (strcmp(id, "sort_roms") == 0) {
    copy_string(settings.sort_roms, sizeof(settings.sort_roms), raw);
  } else {
    set_status(ui, "setting is read-only");
    return 0;
  }
  if (!save_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings write failed");
    return 0;
  }
  update_settings_entries_after_save(ui);
  refresh_runtime_after_setting_save(ui, id);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "saved %s=%s", id, choices[index].display);
  return 1;
}

static int save_manual_time_number(struct ui_state *ui, const char *id,
                                   int direction) {
  long *target = NULL;

  if (!ui || !id || direction == 0) {
    return 0;
  }
  if (!ui->manual_time_initialized) {
    init_manual_time_from_current(ui);
  }
  if (strcmp(id, "system_manual_time_year") == 0) {
    target = &ui->manual_time_year;
  } else if (strcmp(id, "system_manual_time_month") == 0) {
    target = &ui->manual_time_month;
  } else if (strcmp(id, "system_manual_time_day") == 0) {
    target = &ui->manual_time_day;
  } else if (strcmp(id, "system_manual_time_hour") == 0) {
    target = &ui->manual_time_hour;
  } else if (strcmp(id, "system_manual_time_minute") == 0) {
    target = &ui->manual_time_minute;
  }
  if (!target) {
    return 0;
  }
  *target += direction > 0 ? 1 : -1;
  clamp_manual_time_fields(ui);
  update_settings_entries_after_save(ui);
  settings_start_arrow_blink(ui, direction);
  set_status(ui, "manual time draft updated");
  return 1;
}

static int save_setting_number(struct ui_state *ui, const char *id,
                               const char *display_value, int direction) {
  struct frontend_settings settings;
  long value;
  long step = 1;
  long min_value = 0;
  long max_value = 9999;
  const char *system_key = NULL;
  char runtime_status[128];

  if (direction == 0) {
    set_status(ui, "setting needs LEFT/RIGHT");
    return 0;
  }
  if (strncmp(id, "system_manual_time_", 19) == 0 &&
      strcmp(id, "system_manual_time_apply") != 0) {
    return save_manual_time_number(ui, id, direction);
  }
  value = strtol(display_value ? display_value : "0", NULL, 10);
  if (strcmp(id, "rom_scan_slow_threshold_ms") == 0) {
    step = 100;
    min_value = 100;
    max_value = 5000;
  } else if (strcmp(id, "rom_scan_test_file_count") == 0) {
    step = 100;
    min_value = 100;
    max_value = 5000;
  } else if (strcmp(id, "system_volume") == 0) {
    system_key = "volume";
    min_value = 0;
    max_value = 20;
  } else if (strcmp(id, "system_brightness") == 0) {
    system_key = "brightness";
    min_value = 1;
    max_value = 20;
  } else if (strcmp(id, "system_lumination") == 0) {
    system_key = "lumination";
    min_value = 0;
    max_value = 10;
  } else if (strcmp(id, "system_contrast") == 0) {
    system_key = "contrast";
    min_value = 0;
    max_value = 20;
  } else if (strcmp(id, "system_hue") == 0) {
    system_key = "hue";
    min_value = 0;
    max_value = 20;
  } else if (strcmp(id, "system_saturation") == 0) {
    system_key = "saturation";
    min_value = 0;
    max_value = 20;
  } else if (strcmp(id, "theme_transition_ms") == 0) {
    step = 20;
    min_value = 80;
    max_value = 1000;
  } else {
    set_status(ui, "setting is read-only");
    return 0;
  }
  value = clamp_long(value + (direction > 0 ? step : -step), min_value, max_value);
  if (system_key) {
    if (!save_system_config_number(ui, system_key, value)) {
      set_status(ui, "plumOS system config write failed");
      return 0;
    }
    set_device_setting_number(&ui->device, id, value);
    apply_device_runtime_settings(&ui->device, id, runtime_status,
                                  sizeof(runtime_status));
    update_settings_entries_after_save(ui);
    settings_start_arrow_blink(ui, direction);
    if (runtime_status[0]) {
      snprintf(ui->status, sizeof(ui->status), "saved %s=%ld; %s", id, value,
               runtime_status);
    } else {
      snprintf(ui->status, sizeof(ui->status), "saved %s=%ld; runtime applied",
               id, value);
    }
    return 1;
  }

  if (!load_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings read failed");
    return 0;
  }
  if (strcmp(id, "rom_scan_slow_threshold_ms") == 0) {
    settings.rom_scan_slow_threshold_ms = value;
  } else if (strcmp(id, "theme_transition_ms") == 0) {
    settings.graphic_transition_ms = value;
  } else {
    settings.rom_scan_test_file_count = value;
  }
  if (!save_settings(ui->settings_path, &settings)) {
    set_status(ui, "settings write failed");
    return 0;
  }
  update_settings_entries_after_save(ui);
  refresh_runtime_after_setting_save(ui, id);
  settings_start_arrow_blink(ui, direction);
  snprintf(ui->status, sizeof(ui->status), "saved %s=%ld", id, value);
  return 1;
}

static int change_system_volume(struct ui_state *ui, int direction) {
  long value;
  char runtime_status[128];

  if (!ui || direction == 0) {
    return 0;
  }
  value = clamp_long(ui->device.volume + (direction > 0 ? 1 : -1), 0, 20);
  if (value == ui->device.volume) {
    if (!apply_device_runtime_settings(&ui->device, "system_volume",
                                       runtime_status,
                                       sizeof(runtime_status))) {
      snprintf(ui->status, sizeof(ui->status), "Volume %ld/20; apply failed", value);
    } else {
      snprintf(ui->status, sizeof(ui->status), "Volume %ld/20", value);
    }
    return 1;
  }
  if (!save_system_config_number(ui, "volume", value)) {
    set_status(ui, "volume write failed");
    return 0;
  }
  ui->device.volume = value;
  if (!apply_device_runtime_settings(&ui->device, "system_volume", runtime_status,
                                     sizeof(runtime_status))) {
    snprintf(ui->status, sizeof(ui->status), "Volume %ld/20; apply failed", value);
  } else {
    snprintf(ui->status, sizeof(ui->status), "Volume %ld/20", value);
  }
  update_settings_entries_after_save(ui);
  return 1;
}

static int save_brightness_test_value(struct ui_state *ui, long value) {
  char raw[64];

  if (!ui) {
    return 0;
  }
  value = clamp_long(value, 1, 255);
  if (!runtime_lcd_backend_available()) {
    set_status(ui, "brightness test backend unavailable");
    return 0;
  }
  snprintf(raw, sizeof(raw), "%ld\n", value);
  if (!write_text_file(A30_LCD_BACKLIGHT_PATH, raw)) {
    set_status(ui, "brightness test write failed");
    return 0;
  }
  snprintf(ui->status, sizeof(ui->status), "test lcdbl raw=%ld; not saved", value);
  return 1;
}

static int handle_setting_control(struct ui_state *ui, enum ui_action action) {
  const struct setting_entry *entry;
  enum setting_control_type control;
  char id[64];
  char value[256];
  int direction = 0;

  if (!ui || ui->setting_count == 0 || ui->settings_cursor >= ui->setting_count) {
    return 0;
  }
  entry = &ui->setting_entries[ui->settings_cursor];
  copy_string(id, sizeof(id), entry->id);
  copy_string(value, sizeof(value), entry->value);
  control = setting_control_type_for_id(id);
  if (action == ACTION_LEFT) {
    direction = -1;
  } else if (action == ACTION_RIGHT) {
    direction = 1;
  }

  if (control == SETTING_CONTROL_CHECKBOX) {
    int current = bool_from_setting_value(value);
    int next;
    if (!setting_is_writable(id)) {
      set_status(ui, "setting is read-only");
      return 1;
    }
    if (action == ACTION_A) {
      next = !current;
    } else {
      return 0;
    }
    if (strcmp(id, "network_wifi_enabled") == 0) {
      return run_network_wifi_control(ui, next);
    }
    if (strcmp(id, "network_ssh_enabled") == 0) {
      return run_network_service_control(ui, "ssh", next);
    }
    if (strcmp(id, "network_ftp_enabled") == 0) {
      return run_network_service_control(ui, "ftp", next);
    }
    if (strcmp(id, "network_sftp_enabled") == 0) {
      return run_network_service_control(ui, "sftp", next);
    }
    if (strcmp(id, "network_samba_enabled") == 0) {
      return run_network_service_control(ui, "samba", next);
    }
    save_setting_bool(ui, id, next);
    return 1;
  }
  if (control == SETTING_CONTROL_CHOICE && direction != 0) {
    if (!setting_is_writable(id)) {
      set_status(ui, "setting is read-only");
      return 1;
    }
    if (strncmp(id, "performance_", 12) == 0) {
      save_performance_setting_choice(ui, id, direction);
      return 1;
    }
    save_setting_choice(ui, id, value, direction);
    return 1;
  }
  if (control == SETTING_CONTROL_NUMBER && direction != 0) {
    if (!setting_is_writable(id)) {
      set_status(ui, "setting is read-only");
      return 1;
    }
    save_setting_number(ui, id, value, direction);
    return 1;
  }
  if ((action == ACTION_LEFT || action == ACTION_RIGHT) && control != SETTING_CONTROL_ACTION) {
    set_status(ui, "setting is read-only");
    return 1;
  }
  return 0;
}

static int is_network_setting_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "network_rescue") == 0;
}

static int is_ui_theme_settings_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "ui_theme_settings") == 0;
}

static int is_network_connect_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "network_connect_wifi") == 0;
}

static int is_network_services_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "network_services") == 0;
}

static int is_network_information_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "network_information") == 0;
}

static int is_network_usb_disk_mode_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "network_usb_disk_mode") == 0;
}

static int is_system_display_color_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_display_color") == 0;
}

static int is_system_brightness_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_brightness") == 0;
}

static int is_system_time_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_time_settings") == 0;
}

static int is_system_manual_time_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_manual_time") == 0;
}

static int is_system_information_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "system_information") == 0;
}

static int is_help_setting_entry(const struct setting_entry *entry) {
  return entry && strcmp(entry->id, "help") == 0;
}

static int handle_brightness_test_action(struct ui_state *ui, enum ui_action action) {
  size_t cursor;

  if (!ui || ui->settings_category != SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST) {
    return 0;
  }
  if (action == ACTION_B) {
    open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
    select_setting_entry_by_id(ui, "system_brightness");
    set_status(ui, "back to System Settings");
    return 1;
  }
  if (ui->setting_count == 0) {
    return 1;
  }
  cursor = ui->settings_cursor;
  if (cursor >= ui->setting_count) {
    cursor = ui->setting_count - 1;
  }
  if (action == ACTION_LEFT) {
    if (cursor > 0) {
      cursor--;
    }
  } else if (action == ACTION_RIGHT) {
    if (cursor + 1 < ui->setting_count) {
      cursor++;
    }
  } else if (action == ACTION_UP) {
    if (cursor >= BRIGHTNESS_TEST_COLUMNS) {
      cursor -= BRIGHTNESS_TEST_COLUMNS;
    }
  } else if (action == ACTION_DOWN) {
    if (cursor + BRIGHTNESS_TEST_COLUMNS < ui->setting_count) {
      cursor += BRIGHTNESS_TEST_COLUMNS;
    }
  } else if (action == ACTION_A) {
    if (cursor < BRIGHTNESS_TEST_COUNT) {
      save_brightness_test_value(ui, BRIGHTNESS_TEST_VALUES[cursor]);
    }
    return 1;
  } else {
    return 0;
  }
  ui->settings_cursor = cursor;
  return 1;
}

static int handle_wifi_password_key(struct ui_state *ui) {
  if (!ui) {
    return 0;
  }
  wifi_clamp_key_cursor(ui);
  if (ui->wifi_key_row == UI_WIFI_COMMAND_ROW) {
    switch (ui->wifi_key_col) {
    case 0:
      ui->wifi_key_shift = !ui->wifi_key_shift;
      set_status(ui, ui->wifi_key_shift ? "Keyboard uppercase" : "Keyboard lowercase");
      return 1;
    case 1:
      wifi_append_password_char(ui, ' ');
      return 1;
    case 2:
      wifi_delete_password_char(ui);
      return 1;
    case 3:
      ui->wifi_password[0] = '\0';
      set_status(ui, "Password cleared");
      return 1;
    case 4:
      run_wifi_connect_selected(ui);
      return 1;
    default:
      return 1;
    }
  }
  if (ui->wifi_key_row < UI_WIFI_KEYBOARD_ROWS &&
      ui->wifi_key_col < strlen(wifi_keyboard_row_chars(ui, ui->wifi_key_row))) {
    wifi_append_password_char(
        ui, wifi_keyboard_row_chars(ui, ui->wifi_key_row)[ui->wifi_key_col]);
    return 1;
  }
  return 1;
}

static int handle_wifi_connect_action(struct ui_state *ui, enum ui_action action) {
  if (!ui || ui->screen != SCREEN_WIFI_CONNECT) {
    return 0;
  }
  if (ui->wifi_stage == WIFI_CONNECT_RESULT) {
    if (action == ACTION_A || action == ACTION_B) {
      wifi_back_to_network_settings(ui, ui->wifi_result_success
                                            ? "Wi-Fi connection complete"
                                            : "back to Network Settings");
      return 1;
    }
    return 1;
  }
  if (ui->wifi_stage == WIFI_CONNECT_SELECT) {
    if (action == ACTION_UP) {
      if (ui->wifi_cursor > 0) {
        ui->wifi_cursor--;
      }
      return 1;
    }
    if (action == ACTION_DOWN) {
      if (ui->wifi_cursor + 1 < ui->wifi_count) {
        ui->wifi_cursor++;
      }
      return 1;
    }
    if (action == ACTION_RIGHT) {
      ui_cursor_page_down(&ui->wifi_cursor, ui->wifi_count, ui_list_window_size(ui));
      return 1;
    }
    if (action == ACTION_LEFT) {
      ui_cursor_page_up(&ui->wifi_cursor, ui_list_window_size(ui));
      return 1;
    }
    if (action == ACTION_SELECT) {
      set_status(ui, "Rescanning Wi-Fi SSIDs");
      render_ui(ui);
      wifi_scan_networks(ui);
      return 1;
    }
    if (action == ACTION_B) {
      wifi_back_to_network_settings(ui, "back to Network Settings");
      return 1;
    }
    if (action == ACTION_A) {
      if (ui->wifi_count == 0 || ui->wifi_cursor >= ui->wifi_count) {
        set_status(ui, "No SSID selected");
        return 1;
      }
      ui->wifi_password[0] = '\0';
      if (wifi_selected_network_is_open(ui)) {
        run_wifi_connect_selected(ui);
        return 1;
      }
      ui->wifi_stage = WIFI_CONNECT_PASSWORD;
      ui->wifi_key_row = 0;
      ui->wifi_key_col = 0;
      ui->wifi_key_shift = 0;
      set_status(ui, "Enter Wi-Fi password");
      return 1;
    }
    return 1;
  }
  if (ui->wifi_stage == WIFI_CONNECT_PASSWORD) {
    if (action == ACTION_B) {
      ui->wifi_stage = WIFI_CONNECT_SELECT;
      set_status(ui, "back to SSID list");
      return 1;
    }
    if (action == ACTION_UP) {
      if (ui->wifi_key_row > 0) {
        ui->wifi_key_row--;
      }
      wifi_clamp_key_cursor(ui);
      return 1;
    }
    if (action == ACTION_DOWN) {
      if (ui->wifi_key_row + 1 < UI_WIFI_KEYBOARD_ROWS) {
        ui->wifi_key_row++;
      }
      wifi_clamp_key_cursor(ui);
      return 1;
    }
    if (action == ACTION_LEFT) {
      if (ui->wifi_key_col > 0) {
        ui->wifi_key_col--;
      }
      wifi_clamp_key_cursor(ui);
      return 1;
    }
    if (action == ACTION_RIGHT) {
      size_t row_len = wifi_keyboard_row_len(ui, ui->wifi_key_row);
      if (ui->wifi_key_col + 1 < row_len) {
        ui->wifi_key_col++;
      }
      wifi_clamp_key_cursor(ui);
      return 1;
    }
    if (action == ACTION_A) {
      handle_wifi_password_key(ui);
      return 1;
    }
    return 1;
  }
  return 1;
}

static size_t gallery_cursor_after_delta(const struct ui_state *ui,
                                         size_t base_cursor, long delta) {
  size_t next_cursor;

  if (!ui || ui->rom_count == 0) {
    return 0;
  }
  if (base_cursor >= ui->rom_count) {
    base_cursor = ui->rom_count - 1;
  }
  next_cursor = base_cursor;
  if (delta < 0) {
    size_t step = (size_t)(-delta);
    next_cursor = base_cursor > step ? base_cursor - step : 0;
  } else {
    size_t step = (size_t)delta;
    next_cursor = base_cursor + step;
    if (next_cursor >= ui->rom_count) {
      next_cursor = ui->rom_count - 1;
    }
  }
  return next_cursor;
}

static void move_gallery_cursor(struct ui_state *ui, long delta) {
  size_t old_cursor;
  size_t base_cursor;
  size_t next_cursor;

  if (!ui || ui->rom_count == 0 || delta == 0) {
    return;
  }
  if (delta < -1 || delta > 1) {
    next_cursor = gallery_cursor_after_delta(ui, ui->rom_cursor, delta);
    if (next_cursor != ui->rom_cursor) {
      ui->gallery_transition_active = 0;
      ui->gallery_pending_active = 0;
      ui->rom_cursor = next_cursor;
      reset_marquee(ui);
    }
    return;
  }
  if (ui->gallery_transition_active) {
    base_cursor = ui->gallery_transition_to_cursor;
    next_cursor = gallery_cursor_after_delta(ui, base_cursor, delta);
    if (next_cursor != base_cursor) {
      ui->gallery_pending_cursor = next_cursor;
      ui->gallery_pending_active = 1;
    }
    return;
  }
  old_cursor = ui->rom_cursor;
  next_cursor = gallery_cursor_after_delta(ui, old_cursor, delta);
  if (next_cursor != old_cursor) {
    ui_start_gallery_transition(ui, old_cursor, next_cursor);
    ui->rom_cursor = next_cursor;
    reset_marquee(ui);
  }
}

static void handle_action(struct ui_state *ui, enum ui_action action) {
  if (action == ACTION_NONE) {
    return;
  }
  if (action == ACTION_VOLUME_DOWN || action == ACTION_VOLUME_UP) {
    change_system_volume(ui, action == ACTION_VOLUME_UP ? 1 : -1);
    return;
  }
  if (action == ACTION_QUIT) {
    set_status(ui, "quit");
    return;
  }
  if (ui->rescue_network) {
    if (action == ACTION_A) {
      set_status(ui, "Network Recovery is disabled");
    } else if (action == ACTION_B) {
      set_status(ui, "Network Recovery is disabled");
    }
    return;
  }
  if (action == ACTION_FUNCTION) {
    if (ui->screen != SCREEN_SAFE_MENU) {
      open_safe_menu(ui);
    }
    return;
  }

  if (ui->screen == SCREEN_SAFE_MENU) {
    if (action == ACTION_UP) {
      if (ui->safe_cursor > 0) {
        ui->safe_cursor--;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->safe_cursor + 1 < SAFE_ENTRY_COUNT) {
        ui->safe_cursor++;
      }
      return;
    }
    if (action == ACTION_B) {
      close_safe_menu(ui, "safe menu cancelled");
      return;
    }
    if (action == ACTION_A && ui->safe_cursor < SAFE_ENTRY_COUNT) {
      const struct safe_entry *entry = &SAFE_ENTRIES[ui->safe_cursor];
      if (strcmp(entry->id, "cancel") == 0) {
        close_safe_menu(ui, "safe menu cancelled");
        return;
      }
      run_safe_shutdown(ui, entry->id, strcmp(entry->id, "shutdown") == 0);
      ui->screen = ui->safe_back_screen;
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_CORE_SELECT) {
    if (action == ACTION_B) {
      ui->screen = ui->core_back_screen;
      set_status(ui, "close CORE");
      return;
    }
    if (action == ACTION_UP) {
      move_core_menu_cursor(ui, -1);
      return;
    }
    if (action == ACTION_DOWN) {
      move_core_menu_cursor(ui, 1);
      return;
    }
    if (action == ACTION_LEFT) {
      cycle_core_menu_current_row(ui, -1);
      return;
    }
    if (action == ACTION_RIGHT) {
      cycle_core_menu_current_row(ui, 1);
      return;
    }
    if (action == ACTION_SELECT) {
      if (!load_core_select_lines(ui, ui->core_target_system_id,
                                  ui->core_target_relative_path[0]
                                      ? ui->core_target_relative_path
                                      : NULL)) {
        set_status(ui, "cannot refresh core selection");
      } else {
        set_status(ui, "core selection refreshed");
      }
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_SCRAPING) {
    clamp_scraping_menu_cursor(ui);
    if (action == ACTION_UP) {
      ui->scraping_menu_cursor =
          ui->scraping_menu_cursor == 0 ? UI_SCRAPING_FIELD_COUNT - 1
                                        : ui->scraping_menu_cursor - 1;
      set_status(ui, "selected scraping item");
      return;
    }
    if (action == ACTION_DOWN) {
      ui->scraping_menu_cursor =
          ui->scraping_menu_cursor + 1 >= UI_SCRAPING_FIELD_COUNT
              ? 0
              : ui->scraping_menu_cursor + 1;
      set_status(ui, "selected scraping item");
      return;
    }
    if (action == ACTION_LEFT) {
      if (ui->scraping_menu_cursor == UI_SCRAPING_FIELD_IMAGE) {
        cycle_scraping_kind(ui, -1);
        set_status(ui, "selected image type");
      } else if (ui->scraping_menu_cursor == UI_SCRAPING_FIELD_EXISTING) {
        ui->scraping_replace_existing = !ui->scraping_replace_existing;
        set_status(ui, "selected existing image mode");
      } else {
        if (ui->scraping_choice_cursor == 0) {
          ui->scraping_choice_cursor = ui->scraping_choice_count;
        } else {
          ui->scraping_choice_cursor--;
        }
        set_status(ui, "selected scraping system");
      }
      return;
    }
    if (action == ACTION_RIGHT) {
      if (ui->scraping_menu_cursor == UI_SCRAPING_FIELD_IMAGE) {
        cycle_scraping_kind(ui, 1);
        set_status(ui, "selected image type");
      } else if (ui->scraping_menu_cursor == UI_SCRAPING_FIELD_EXISTING) {
        ui->scraping_replace_existing = !ui->scraping_replace_existing;
        set_status(ui, "selected existing image mode");
      } else {
        if (ui->scraping_choice_cursor >= ui->scraping_choice_count) {
          ui->scraping_choice_cursor = 0;
        } else {
          ui->scraping_choice_cursor++;
        }
        set_status(ui, "selected scraping system");
      }
      return;
    }
    if (action == ACTION_A) {
      run_scraping_action(ui);
      return;
    }
    if (action == ACTION_SELECT) {
      open_thumbnail_results_screen(ui);
      return;
    }
    if (action == ACTION_B) {
      open_apps_menu(ui);
      select_menu_entry_by_id(ui, "scraping");
      set_status(ui, "back to Apps");
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_THUMBNAIL_RESULTS) {
    if (action == ACTION_UP) {
      if (ui->thumbnail_result_cursor > 0) {
        ui->thumbnail_result_cursor--;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->thumbnail_result_cursor + 1 < ui->thumbnail_result_count) {
        ui->thumbnail_result_cursor++;
      }
      return;
    }
    if (action == ACTION_RIGHT) {
      ui_cursor_page_down(&ui->thumbnail_result_cursor, ui->thumbnail_result_count,
                          UI_THUMBNAIL_RESULT_WINDOW);
      return;
    }
    if (action == ACTION_LEFT) {
      ui_cursor_page_up(&ui->thumbnail_result_cursor, UI_THUMBNAIL_RESULT_WINDOW);
      return;
    }
    if (action == ACTION_B) {
      open_apps_menu(ui);
      select_menu_entry_by_id(ui, "scraping");
      return;
    }
    if (action == ACTION_A || action == ACTION_SELECT) {
      if (!load_thumbnail_results(ui)) {
        set_status(ui, "cannot refresh scraping results");
      } else {
        set_status(ui, "scraping results refreshed");
      }
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_NETWORK_RESCUE) {
    if (action == ACTION_A) {
      set_status(ui, "Network Recovery is disabled");
      return;
    }
    if (action == ACTION_B) {
      ui->screen = SCREEN_START_MENU;
      set_status(ui, "back to START");
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_WIFI_CONNECT) {
    handle_wifi_connect_action(ui, action);
    return;
  }

  if (ui->screen == SCREEN_USB_DISK_CONFIRM) {
    if (action == ACTION_A) {
      open_usb_disk_starting_screen(ui);
      return;
    }
    if (action == ACTION_B) {
      open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK_SERVICE);
      select_setting_entry_by_id(ui, "network_usb_disk_mode");
      set_status(ui, "back to NW Service");
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_USB_DISK_STARTING) {
    return;
  }

  if (ui->screen == SCREEN_GALLERY) {
    if (action == ACTION_LEFT) {
      move_gallery_cursor(ui, -1);
      return;
    }
    if (action == ACTION_RIGHT) {
      move_gallery_cursor(ui, 1);
      return;
    }
    if (action == ACTION_UP) {
      move_gallery_cursor(ui, -5);
      return;
    }
    if (action == ACTION_DOWN) {
      move_gallery_cursor(ui, 5);
      return;
    }
    if (action == ACTION_B) {
      ui->rom_entry_screen = SCREEN_GALLERY;
      ui->screen = SCREEN_TOP;
      ui->gallery_transition_active = 0;
      ui->gallery_pending_active = 0;
      set_status(ui, "back to TOP");
      reset_marquee(ui);
      return;
    }
    if (action == ACTION_X) {
      ui->rom_entry_screen = SCREEN_ROMS;
      ui->screen = (ui->gallery_back_screen == SCREEN_FAVORITES ||
                    ui->gallery_back_screen == SCREEN_RECENT)
                       ? ui->gallery_back_screen
                       : SCREEN_ROMS;
      ui->gallery_transition_active = 0;
      ui->gallery_pending_active = 0;
      set_status(ui, "back to ROM list");
      reset_marquee(ui);
      return;
    }
    if (action == ACTION_A && ui->rom_count > 0) {
      const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
      launch_rom_entry(ui, entry);
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    if (action == ACTION_SELECT && ui->rom_count > 0) {
      const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
      open_core_select_screen(ui, entry->system_id[0] ? entry->system_id
                                                      : ui->current_system_id,
                              entry->relative_path);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_TOP) {
    if (ui_uses_graphic_mode(ui) &&
        (action == ACTION_UP || action == ACTION_DOWN ||
         action == ACTION_LEFT || action == ACTION_RIGHT)) {
      ui_move_graphic_top_cursor(ui, action);
      return;
    }
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
    if (action == ACTION_RIGHT) {
      ui_cursor_page_down(&ui->top_cursor, ui->top_count, ui_list_window_size(ui));
      return;
    }
    if (action == ACTION_LEFT) {
      ui_cursor_page_up(&ui->top_cursor, ui_list_window_size(ui));
      return;
    }
    if (action == ACTION_A && ui->top_count > 0) {
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
      open_core_select_screen(ui, entry->id, NULL);
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
    if (action == ACTION_B) {
      if (strcmp(ui->menu_id, "apps") == 0) {
        if (!load_start_menu_entries(ui)) {
          set_status(ui, "cannot load START menu");
        } else {
          select_menu_entry_by_id(ui, "apps");
          set_status(ui, "back to START");
        }
        return;
      }
      ui->screen = ui->back_screen;
      set_status(ui, "close START menu");
      return;
    }
    if (action == ACTION_A) {
      const struct menu_entry *entry;
      if (ui->menu_count == 0) {
        return;
      }
      entry = &ui->menu_entries[ui->menu_cursor];
      if (strcmp(entry->action, "internal:settings") == 0 ||
          strcmp(entry->action, "internal:ui-settings") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_UI);
      } else if (strcmp(entry->action, "internal:system-settings") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
      } else if (strcmp(entry->action, "internal:network-settings") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK);
      } else if (strcmp(entry->action, "internal:performance-settings") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_PERFORMANCE);
      } else if (strcmp(entry->action, "internal:favorites") == 0) {
        open_favorites_screen(ui);
      } else if (strcmp(entry->action, "internal:recent") == 0) {
        open_recent_screen(ui);
      } else if (strcmp(entry->action, "internal:network-recovery") == 0) {
        set_status(ui, "Network Recovery is disabled");
      } else if (strcmp(entry->action, "internal:network") == 0) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK);
      } else if (strcmp(entry->action, "internal:help") == 0) {
        open_help_screen(ui);
      } else if (strcmp(entry->action, "internal:scraping") == 0) {
        open_scraping_screen(ui);
      } else if (strcmp(entry->action, "internal:thumbnail-results") == 0) {
        open_thumbnail_results_screen(ui);
      } else if (strcmp(entry->action, "system:shutdown") == 0) {
        run_safe_shutdown(ui, "shutdown", 1);
      } else if (strcmp(entry->action, "menu:apps") == 0) {
        open_apps_menu(ui);
      } else if (strncmp(entry->action, "shell:", 6) == 0) {
        run_menu_shell_action(ui, entry);
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
    if (handle_brightness_test_action(ui, action)) {
      return;
    }
    if (action == ACTION_UP) {
      if (ui->settings_cursor > 0) {
        ui->settings_cursor--;
        ui->settings_blink_direction = 0;
        ui->settings_blink_until_ms = 0;
      }
      return;
    }
    if (action == ACTION_DOWN) {
      if (ui->settings_cursor + 1 < ui->setting_count) {
        ui->settings_cursor++;
        ui->settings_blink_direction = 0;
        ui->settings_blink_until_ms = 0;
      }
      return;
    }
    if (action == ACTION_LEFT || action == ACTION_RIGHT) {
      handle_setting_control(ui, action);
      return;
    }
    if (action == ACTION_B) {
      if (ui->settings_category == SETTINGS_CATEGORY_UI_THEME) {
        open_settings_screen(ui, SETTINGS_CATEGORY_UI);
        select_setting_entry_by_id(ui, "ui_theme_settings");
        set_status(ui, "back to UI Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_DISPLAY_COLOR) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
        select_setting_entry_by_id(ui, "system_display_color");
        set_status(ui, "back to System Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_TIME) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
        select_setting_entry_by_id(ui, "system_time_settings");
        set_status(ui, "back to System Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_TIME_MANUAL) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_TIME);
        select_setting_entry_by_id(ui, "system_manual_time");
        set_status(ui, "back to Time Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_SYSTEM_INFORMATION) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM);
        select_setting_entry_by_id(ui, "system_information");
        set_status(ui, "back to System Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_NETWORK_INFORMATION) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK);
        select_setting_entry_by_id(ui, "network_information");
        set_status(ui, "back to Network Settings");
        return;
      }
      if (ui->settings_category == SETTINGS_CATEGORY_NETWORK_SERVICE) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK);
        select_setting_entry_by_id(ui, "network_services");
        set_status(ui, "back to Network Settings");
        return;
      }
      ui->screen = SCREEN_START_MENU;
      set_status(ui, "back to START");
      return;
    }
    if (action == ACTION_A && ui->setting_count > 0) {
      const struct setting_entry *entry = &ui->setting_entries[ui->settings_cursor];
      char msg[256];
      if (handle_setting_control(ui, action)) {
        return;
      }
      if (is_ui_theme_settings_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_UI_THEME);
        return;
      }
      if (is_network_connect_entry(entry)) {
        open_wifi_connect_screen(ui);
        return;
      }
      if (is_network_services_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK_SERVICE);
        return;
      }
      if (is_network_setting_entry(entry)) {
        set_status(ui, "Network Recovery is disabled");
        return;
      }
      if (is_network_usb_disk_mode_entry(entry)) {
        open_usb_disk_confirm_screen(ui);
        return;
      }
      if (is_network_information_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_NETWORK_INFORMATION);
        return;
      }
      if (is_system_display_color_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_DISPLAY_COLOR);
        return;
      }
      if (is_system_time_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_TIME);
        return;
      }
      if (is_system_manual_time_entry(entry)) {
        init_manual_time_from_current(ui);
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_TIME_MANUAL);
        return;
      }
      if (strcmp(entry->id, "system_manual_time_apply") == 0) {
        apply_manual_system_time(ui);
        return;
      }
      if (is_system_brightness_entry(entry)) {
        if (brightness_test_tiles_enabled()) {
          open_system_brightness_test_screen(ui);
        } else {
          set_status(ui, "Brightness changes with LEFT/RIGHT");
        }
        return;
      }
      if (is_system_information_entry(entry)) {
        open_settings_screen(ui, SETTINGS_CATEGORY_SYSTEM_INFORMATION);
        return;
      }
      if (strcmp(entry->id, "performance_clear_cpu_override") == 0) {
        clear_performance_cpu_override(ui);
        return;
      }
      if (is_help_setting_entry(entry)) {
        open_help_screen(ui);
        return;
      }
      snprintf(msg, sizeof(msg), "read-only: %s", entry->display_name);
      set_status(ui, msg);
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (ui->screen == SCREEN_HELP) {
    if (action == ACTION_B) {
      ui->screen = SCREEN_START_MENU;
      set_status(ui, "back to START");
      return;
    }
    if (action == ACTION_START) {
      open_start_menu(ui);
      return;
    }
    return;
  }

  if (action == ACTION_X && ui_uses_graphic_mode(ui) &&
      (ui->screen == SCREEN_ROMS || ui->screen == SCREEN_FAVORITES ||
       ui->screen == SCREEN_RECENT)) {
    open_gallery_screen(ui);
    return;
  }

  if (action == ACTION_UP) {
    size_t old_cursor = ui->rom_cursor;
    if (ui->rom_cursor > 0) {
      ui->rom_cursor--;
    }
    if (ui->rom_cursor != old_cursor) {
      reset_marquee(ui);
    }
    return;
  }
  if (action == ACTION_DOWN) {
    size_t old_cursor = ui->rom_cursor;
    if (ui->rom_cursor + 1 < ui->rom_count) {
      ui->rom_cursor++;
    }
    if (ui->rom_cursor != old_cursor) {
      reset_marquee(ui);
    }
    return;
  }
  if (action == ACTION_RIGHT) {
    size_t old_cursor = ui->rom_cursor;
    ui_cursor_page_down(&ui->rom_cursor, ui->rom_count, ui_list_window_size(ui));
    if (ui->rom_cursor != old_cursor) {
      reset_marquee(ui);
    }
    return;
  }
  if (action == ACTION_LEFT) {
    size_t old_cursor = ui->rom_cursor;
    ui_cursor_page_up(&ui->rom_cursor, ui_list_window_size(ui));
    if (ui->rom_cursor != old_cursor) {
      reset_marquee(ui);
    }
    return;
  }
  if (action == ACTION_B) {
    ui->rom_entry_screen = SCREEN_ROMS;
    ui->screen = SCREEN_TOP;
    set_status(ui, "back to TOP");
    return;
  }
  if (action == ACTION_A && ui->rom_count > 0) {
    const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
    launch_rom_entry(ui, entry);
    return;
  }
  if (action == ACTION_START) {
    open_start_menu(ui);
    return;
  }
  if (action == ACTION_SELECT && ui->rom_count > 0) {
    const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
    open_core_select_screen(ui, entry->system_id[0] ? entry->system_id : ui->current_system_id,
                            entry->relative_path);
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
  case 9:
    return ACTION_B;
  case KEY_LEFTSHIFT:
  case BTN_NORTH:
  case KEY_X:
    return ACTION_X;
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
  case KEY_VOLUMEDOWN:
    return ACTION_VOLUME_DOWN;
  case KEY_VOLUMEUP:
    return ACTION_VOLUME_UP;
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
  case 'x':
  case 'X':
    return ACTION_X;
  case '-':
  case '[':
    return ACTION_VOLUME_DOWN;
  case '+':
  case '=':
  case ']':
    return ACTION_VOLUME_UP;
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
  if (strcmp(token, "x") == 0 || strcmp(token, "gallery") == 0) {
    return ACTION_X;
  }
  if (strcmp(token, "function") == 0 || strcmp(token, "safe") == 0) {
    return ACTION_FUNCTION;
  }
  if (strcmp(token, "volume_down") == 0 || strcmp(token, "volumedown") == 0 ||
      strcmp(token, "voldown") == 0) {
    return ACTION_VOLUME_DOWN;
  }
  if (strcmp(token, "volume_up") == 0 || strcmp(token, "volumeup") == 0 ||
      strcmp(token, "volup") == 0) {
    return ACTION_VOLUME_UP;
  }
  if (strcmp(token, "q") == 0 || strcmp(token, "quit") == 0) {
    return ACTION_QUIT;
  }
  return ACTION_NONE;
}

static int settings_value_action_repeats(const struct ui_state *ui,
                                         enum ui_action action) {
  const struct setting_entry *entry;
  enum setting_control_type control;

  if (!ui || ui->screen != SCREEN_SETTINGS ||
      (action != ACTION_LEFT && action != ACTION_RIGHT) ||
      ui->setting_count == 0 || ui->settings_cursor >= ui->setting_count) {
    return 0;
  }
  entry = &ui->setting_entries[ui->settings_cursor];
  control = setting_control_type_for_id(entry->id);
  if (control != SETTING_CONTROL_NUMBER && control != SETTING_CONTROL_CHOICE) {
    return 0;
  }
  return setting_is_writable(entry->id) ||
         strncmp(entry->id, "performance_", 12) == 0;
}

static int action_repeat_interval_ms(const struct ui_state *ui,
                                     enum ui_action action) {
  if (action == ACTION_UP || action == ACTION_DOWN) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (action == ACTION_VOLUME_DOWN || action == ACTION_VOLUME_UP) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (ui && ui->screen == SCREEN_WIFI_CONNECT &&
      ui->wifi_stage == WIFI_CONNECT_PASSWORD &&
      (action == ACTION_LEFT || action == ACTION_RIGHT)) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (ui && ui->screen == SCREEN_TOP && ui_graphic_top_uses_strip(ui) &&
      (action == ACTION_LEFT || action == ACTION_RIGHT)) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (ui && ui->screen == SCREEN_GALLERY &&
      (action == ACTION_LEFT || action == ACTION_RIGHT)) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (ui && ui->screen == SCREEN_SETTINGS &&
      ui->settings_category == SETTINGS_CATEGORY_SYSTEM_BRIGHTNESS_TEST &&
      (action == ACTION_LEFT || action == ACTION_RIGHT)) {
    return UI_KEY_REPEAT_INTERVAL_MS;
  }
  if (settings_value_action_repeats(ui, action)) {
    return UI_SETTING_VALUE_REPEAT_INTERVAL_MS;
  }
  return 0;
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

static void drain_input_fd(int fd) {
  struct input_event ev;
  if (fd < 0) {
    return;
  }
  while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
  }
}

static void settle_input_after_child(struct ui_state *ui) {
  long long deadline;
  long long quiet_until;
  long long now;
  int fd;

  if (!ui) {
    return;
  }
  fd = ui->input_event_fd;
  if (fd < 0) {
    ui->ignore_input_until_ms = current_time_ms() + 750;
    return;
  }

  drain_input_fd(fd);
  now = current_time_ms();
  deadline = now + 1500;
  quiet_until = now + 250;

  while ((now = current_time_ms()) < deadline && now < quiet_until) {
    struct pollfd pfd;
    int timeout_ms = (int)(quiet_until - now);
    long long remaining_ms = deadline - now;
    if (timeout_ms > remaining_ms) {
      timeout_ms = (int)remaining_ms;
    }
    if (timeout_ms < 0) {
      timeout_ms = 0;
    }
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLIN)) {
      drain_input_fd(fd);
      quiet_until = current_time_ms() + 250;
    }
  }

  ui->ignore_input_until_ms = current_time_ms() + 750;
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
  return ui->render_failed ? 0 : 1;
}

static int ui_needs_periodic_refresh(const struct ui_state *ui) {
  if (!ui) {
    return 0;
  }
  if (ui->rescue_network) {
    return 1;
  }
  if (ui->renderer_mali && ui->top_transition_active) {
    return 1;
  }
  if (ui->renderer_mali && ui->gallery_transition_active) {
    return 1;
  }
  if (ui_uses_graphic_mode(ui) && ui->screen == SCREEN_GALLERY) {
    return 1;
  }
  if (ui->rom_scan_refresh_pid > 0) {
    return 1;
  }
  if (ui_uses_graphic_mode(ui) && ui->screen == SCREEN_TOP) {
    return 0;
  }
  return ui->renderer_mali;
}

static int ui_periodic_refresh_interval_ms(const struct ui_state *ui) {
  if (!ui) {
    return 0;
  }
  if (ui->renderer_mali && ui->top_transition_active) {
    return 16;
  }
  if (ui->renderer_mali && ui->gallery_transition_active) {
    return 16;
  }
  if (ui_uses_graphic_mode(ui) && ui->screen == SCREEN_GALLERY) {
    return 16;
  }
  if (ui->renderer_mali) {
    return 100;
  }
  if (ui->rom_scan_refresh_pid > 0) {
    return 250;
  }
  if (ui->rescue_network) {
    return 1000;
  }
  return 0;
}

static int run_event_loop(struct ui_state *ui, const char *event_path) {
  int event_fd = -1;
  int stdin_fd = STDIN_FILENO;
  int stdin_active = 1;
  int old_flags = -1;
  time_t deadline = 0;
  long long next_refresh_ms = 0;

  if (event_path && event_path[0]) {
    event_fd = open(event_path, O_RDONLY | O_NONBLOCK);
    if (event_fd < 0) {
      snprintf(ui->status, sizeof(ui->status), "input open failed: %s", strerror(errno));
    }
  }
  ui->input_event_fd = event_fd;
  old_flags = fcntl(stdin_fd, F_GETFL, 0);
  if (old_flags >= 0) {
    fcntl(stdin_fd, F_SETFL, old_flags | O_NONBLOCK);
  } else {
    stdin_active = 0;
  }
  if (ui->timeout_sec > 0) {
    deadline = time(NULL) + ui->timeout_sec;
  }

  render_ui(ui);
  mark_frontend_ready_if_needed(ui);
  if (ui_needs_periodic_refresh(ui)) {
    next_refresh_ms = current_time_ms() + ui_periodic_refresh_interval_ms(ui);
  }
  while (1) {
    struct pollfd pfds[2];
    nfds_t count = 0;
    nfds_t event_index = (nfds_t)-1;
    nfds_t stdin_index = (nfds_t)-1;
    int rc;
    enum ui_action action = ACTION_NONE;
    long long now_ms;
    int poll_timeout = ui->repeat_action != ACTION_NONE ? 50 :
                       (ui_needs_periodic_refresh(ui) ? 60 : 250);

    now_ms = current_time_ms();
    if (poll_rom_scan_refresh(ui)) {
      render_ui(ui);
      if (ui_needs_periodic_refresh(ui)) {
        next_refresh_ms = current_time_ms() + ui_periodic_refresh_interval_ms(ui);
      } else {
        next_refresh_ms = 0;
      }
      continue;
    }
    if (ui->screen == SCREEN_USB_DISK_STARTING && ui->usb_disk_start_due_ms > 0) {
      if (now_ms >= ui->usb_disk_start_due_ms) {
        run_usb_disk_mode(ui);
        render_ui(ui);
        if (ui_needs_periodic_refresh(ui)) {
          next_refresh_ms = current_time_ms() + ui_periodic_refresh_interval_ms(ui);
        }
        continue;
      }
      if (ui->usb_disk_start_due_ms - now_ms < poll_timeout) {
        poll_timeout = (int)(ui->usb_disk_start_due_ms - now_ms);
      }
    }
    if (ui_needs_periodic_refresh(ui) && next_refresh_ms > 0) {
      long long refresh_due_ms = next_refresh_ms - now_ms;
      if (refresh_due_ms <= 0) {
        poll_timeout = 0;
      } else if (refresh_due_ms < poll_timeout) {
        poll_timeout = (int)refresh_due_ms;
      }
    }

    if (event_fd >= 0) {
      event_index = count;
      pfds[count].fd = event_fd;
      pfds[count].events = POLLIN;
      pfds[count].revents = 0;
      count++;
    }
    if (stdin_active) {
      stdin_index = count;
      pfds[count].fd = stdin_fd;
      pfds[count].events = POLLIN | POLLHUP | POLLERR | POLLNVAL;
      pfds[count].revents = 0;
      count++;
    }

    if (ui->timeout_sec > 0 && time(NULL) >= deadline) {
      break;
    }
    rc = poll(pfds, count, poll_timeout);
    if (rc < 0 && errno != EINTR) {
      break;
    }
    if (rc <= 0) {
      if (ui->repeat_action != ACTION_NONE &&
          current_time_ms() >= ui->repeat_next_ms) {
        int repeat_interval_ms = action_repeat_interval_ms(ui, ui->repeat_action);
        if (repeat_interval_ms > 0) {
          action = ui->repeat_action;
          ui->repeat_next_ms = current_time_ms() + repeat_interval_ms;
        } else {
          ui->repeat_action = ACTION_NONE;
          ui->repeat_key_code = 0;
          ui->repeat_next_ms = 0;
        }
      }
      if (action != ACTION_NONE) {
        handle_action(ui, action);
        render_ui(ui);
        if (ui_needs_periodic_refresh(ui)) {
          next_refresh_ms = current_time_ms() + ui_periodic_refresh_interval_ms(ui);
        }
        continue;
      }
      if (ui_needs_periodic_refresh(ui)) {
        now_ms = current_time_ms();
        if (next_refresh_ms <= 0 || now_ms >= next_refresh_ms) {
          render_ui(ui);
          next_refresh_ms = now_ms + ui_periodic_refresh_interval_ms(ui);
        }
      }
      continue;
    }

    if (ui->ignore_input_until_ms > current_time_ms()) {
      if (event_index != (nfds_t)-1 && (pfds[event_index].revents & POLLIN)) {
        drain_input_fd(event_fd);
      }
      if (ui_needs_periodic_refresh(ui)) {
        now_ms = current_time_ms();
        if (next_refresh_ms <= 0 || now_ms >= next_refresh_ms) {
          render_ui(ui);
          next_refresh_ms = now_ms + ui_periodic_refresh_interval_ms(ui);
        }
      }
      continue;
    }

    if (event_index != (nfds_t)-1 && (pfds[event_index].revents & POLLIN)) {
      struct input_event ev;
      while (read(event_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_KEY) {
          enum ui_action event_action = action_from_key_code(ev.code);
          if (ev.value == 0) {
            if (ui->repeat_action != ACTION_NONE && ui->repeat_key_code == ev.code) {
              ui->repeat_action = ACTION_NONE;
              ui->repeat_key_code = 0;
              ui->repeat_next_ms = 0;
            }
            continue;
          }
          if (ev.value == 1 || ev.value == 2) {
            int repeat_interval_ms;
            action = event_action;
            repeat_interval_ms = action_repeat_interval_ms(ui, event_action);
            if (repeat_interval_ms > 0) {
              ui->repeat_action = event_action;
              ui->repeat_key_code = ev.code;
              ui->repeat_next_ms = current_time_ms() +
                                   (ev.value == 1 ? UI_KEY_REPEAT_DELAY_MS
                                                  : repeat_interval_ms);
            }
          }
          if (event_action == ACTION_NONE && ev.value == 1) {
            snprintf(ui->status, sizeof(ui->status), "unmapped key code=%u value=%d", ev.code,
                     ev.value);
          }
        }
      }
    }
    if (stdin_index != (nfds_t)-1 &&
        (pfds[stdin_index].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))) {
      if (pfds[stdin_index].revents & POLLIN) {
        char buf[32];
        ssize_t n = read(stdin_fd, buf, sizeof(buf));
        ssize_t i;
        if (n > 0) {
          for (i = 0; i < n; i++) {
            enum ui_action stdin_action =
                action_from_stdin_char((unsigned char)buf[i]);
            if (stdin_action != ACTION_NONE) {
              action = stdin_action;
            }
          }
        } else {
          stdin_active = 0;
        }
      } else {
        stdin_active = 0;
      }
    }

    if (action == ACTION_NONE && ui->repeat_action != ACTION_NONE &&
        current_time_ms() >= ui->repeat_next_ms) {
      int repeat_interval_ms = action_repeat_interval_ms(ui, ui->repeat_action);
      if (repeat_interval_ms > 0) {
        action = ui->repeat_action;
        ui->repeat_next_ms = current_time_ms() + repeat_interval_ms;
      } else {
        ui->repeat_action = ACTION_NONE;
        ui->repeat_key_code = 0;
        ui->repeat_next_ms = 0;
      }
    }

    if (action == ACTION_NONE && ui_needs_periodic_refresh(ui)) {
      now_ms = current_time_ms();
      if (next_refresh_ms <= 0 || now_ms >= next_refresh_ms) {
        render_ui(ui);
        next_refresh_ms = now_ms + ui_periodic_refresh_interval_ms(ui);
      }
    }

    if (action != ACTION_NONE) {
      handle_action(ui, action);
      render_ui(ui);
      if (ui_needs_periodic_refresh(ui)) {
        next_refresh_ms = current_time_ms() + ui_periodic_refresh_interval_ms(ui);
      }
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
  ui->input_event_fd = -1;
  return ui->render_failed ? 1 : 0;
}

static void usage(const char *argv0) {
  printf("Usage:\n");
  printf("  %s [--all] [--refresh] [--once] [--timeout SEC] [--event PATH]\n", argv0);
  printf("     [--renderer text|mali] [--fb PATH] [--egl-lib PATH] [--gles-lib PATH]\n");
  printf("     [--rotation auto|none|cw|ccw] [--font PATH]\n");
  printf("     [--tty-entry-scale 1|1.5|2]\n");
  printf("     [--rescue-network]  # disabled compatibility screen\n");
  printf("  %s --script up,down,a,b,x,select,start,function,volume_up,volume_down,q [--no-clear]\n", argv0);
  printf("  %s --dump-events [--timeout SEC] [--event PATH]\n", argv0);
  printf("\n");
  printf("Keyboard fallback over SSH: w/s/a/d, e or space for A, b, x, m, c, f, +/- for volume, q.\n");
  printf("Environment:\n");
  printf("  PLUMOS_SDCARD_ROOT  Default: /mnt/SDCARD\n");
  printf("  PLUMOS_ROOT         Default: $PLUMOS_SDCARD_ROOT/plumos\n");
  printf("  PLUMOS_INPUT_EVENT  Default: auto-detect gpio-keys-polled\n");
  printf("  PLUMOS_RENDERER     text or mali. Default: text\n");
  printf("  PLUMOS_FB           Default for Mali renderer: /dev/fb0\n");
  printf("  PLUMOS_EGL_LIB      Default for Mali renderer: /usr/lib/libEGL.so\n");
  printf("  PLUMOS_GLES_LIB     Default for Mali renderer: /usr/lib/libGLESv2.so\n");
  printf("  PLUMOS_MALI_ROTATION auto, none, cw, or ccw. Default: auto\n");
  printf("  PLUMOS_MALI_FONT    Optional FreeType font for non-ASCII Mali text\n");
  printf("  PLUMOS_MALI_TTY_ENTRY_SCALE  1, 1.5, or 2. Default: 1\n");
  printf("  PLUMOS_MALI_SWAP_INTERVAL  0 or 1. Default: 1\n");
  printf("  PLUMOS_CONTROLLER_RESCUE network opens a disabled compatibility screen\n");
  printf("  PLUMOS_SYSTEM_SETTINGS_JSON  Default: $PLUMOS_ROOT/config/system/settings.json\n");
  printf("  PLUMOS_A30_WPA_STATUS   Default: /tmp/wpa_status.txt\n");
  printf("  PLUMOS_CONTROLLER_CPU_DEFAULT  648MHz/2-core FE default; set 0 to skip\n");
}

static int run_boot_resume_if_needed(struct ui_state *ui,
                                     const struct frontend_settings *settings) {
  char text_ui[PATH_MAX];
  char log_dir[PATH_MAX];
  char log_path[PATH_MAX];
  char cmd[UI_COMMAND_MAX];
  const char *frontend_mode;
  size_t pos = 0;
  int rc;

  if (!ui || !settings) {
    return 0;
  }
  frontend_mode = getenv("PLUMOS_FRONTEND_MODE");
  if ((frontend_mode && strcmp(frontend_mode, "manual") == 0) ||
      strcmp(settings->boot_resume_mode, "off") == 0) {
    return 0;
  }
  if (strcmp(settings->boot_resume_mode, "last") != 0) {
    return 0;
  }
  if (!join_path(text_ui, sizeof(text_ui), ui->plumos_root, "bin/plumos-text-ui") ||
      !join_path(log_dir, sizeof(log_dir), ui->plumos_root, "logs") ||
      !join_path(log_path, sizeof(log_path), log_dir, "frontend-boot-resume.log")) {
    set_status(ui, "boot resume path too long");
    return 0;
  }
  if (!file_exists(text_ui)) {
    set_status(ui, "boot resume skipped; plumos-text-ui missing");
    return 0;
  }

  cmd[0] = '\0';
  if (!append_string(cmd, sizeof(cmd), &pos, "mkdir -p ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_dir) ||
      !append_string(cmd, sizeof(cmd), &pos, "; PLUMOS_SDCARD_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->sdcard_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " PLUMOS_ROOT=") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, ui->plumos_root) ||
      !append_string(cmd, sizeof(cmd), &pos, " ") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, text_ui) ||
      !append_string(cmd, sizeof(cmd), &pos, " boot --execute >>") ||
      !append_shell_quoted(cmd, sizeof(cmd), &pos, log_path) ||
      !append_string(cmd, sizeof(cmd), &pos, " 2>&1")) {
    set_status(ui, "boot resume command too long");
    return 0;
  }

  rc = system(cmd);
  if (rc == -1) {
    set_status(ui, "boot resume system call failed");
    return 0;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    set_status(ui, "boot resume checked; TOP ready");
    return 1;
  }
  set_status(ui, "boot resume failed; see frontend-boot-resume.log");
  return 0;
}

int main(int argc, char **argv) {
  static struct ui_state ui;
  const char *plumos_root_env;
  const char *renderer_env;
  const char *system_config_env;
  const char *wpa_status_env;
  const char *fb_path;
  const char *egl_path;
  const char *gles_path;
  const char *rotation_env;
  const char *mali_font_env;
  const char *mali_tty_entry_scale_env;
  const char *script = NULL;
  char event_path[PATH_MAX];
  struct frontend_settings initial_settings;
  int initial_settings_loaded = 0;
  int startup_resume_allowed = 0;
  int dump_events = 0;
  int exit_code = 0;
  int i;

  memset(&ui, 0, sizeof(ui));
  ui.input_event_fd = -1;
  ui.rom_entry_screen = SCREEN_ROMS;
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
  renderer_env = getenv("PLUMOS_RENDERER");
  if (renderer_env && strcmp(renderer_env, "mali") == 0) {
    ui.renderer_mali = 1;
    ui.no_clear = 1;
  }
  discover_input_event(event_path, sizeof(event_path));
  if (getenv("PLUMOS_INPUT_EVENT") && getenv("PLUMOS_INPUT_EVENT")[0]) {
    copy_string(event_path, sizeof(event_path), getenv("PLUMOS_INPUT_EVENT"));
  }
  system_config_env = getenv("PLUMOS_SYSTEM_SETTINGS_JSON");
  if (system_config_env && system_config_env[0] &&
      !copy_string(ui.system_config_path, sizeof(ui.system_config_path), system_config_env)) {
    fprintf(stderr, "error: plumOS system settings path is too long\n");
    return 1;
  }
  wpa_status_env = getenv("PLUMOS_A30_WPA_STATUS");
  if (!copy_string(ui.wpa_status_path, sizeof(ui.wpa_status_path),
                   wpa_status_env && wpa_status_env[0] ? wpa_status_env
                                                       : "/tmp/wpa_status.txt")) {
    fprintf(stderr, "error: A30 WPA status path is too long\n");
    return 1;
  }
  fb_path = getenv("PLUMOS_FB");
  egl_path = getenv("PLUMOS_EGL_LIB");
  gles_path = getenv("PLUMOS_GLES_LIB");
  rotation_env = getenv("PLUMOS_MALI_ROTATION");
  mali_font_env = getenv("PLUMOS_MALI_FONT");
  mali_tty_entry_scale_env = getenv("PLUMOS_MALI_TTY_ENTRY_SCALE");
  copy_string(ui.mali_rotation, sizeof(ui.mali_rotation),
              rotation_env && rotation_env[0] ? rotation_env : "auto");
  copy_string(ui.mali_tty_entry_scale, sizeof(ui.mali_tty_entry_scale),
              mali_tty_entry_scale_env && mali_tty_entry_scale_env[0]
                  ? mali_tty_entry_scale_env
                  : "1");
  if (getenv("PLUMOS_CONTROLLER_RESCUE") &&
      strcmp(getenv("PLUMOS_CONTROLLER_RESCUE"), "network") == 0) {
    ui.rescue_network = 1;
    copy_string(ui.status, sizeof(ui.status), "Network Recovery is disabled");
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
    } else if (strcmp(argv[i], "--mali") == 0) {
      ui.renderer_mali = 1;
      ui.no_clear = 1;
    } else if (strcmp(argv[i], "--renderer") == 0 && i + 1 < argc) {
      const char *renderer = argv[++i];
      if (strcmp(renderer, "mali") == 0) {
        ui.renderer_mali = 1;
        ui.no_clear = 1;
      } else if (strcmp(renderer, "text") == 0) {
        ui.renderer_mali = 0;
      } else {
        fprintf(stderr, "error: unknown renderer: %s\n", renderer);
        return 2;
      }
    } else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
      ui.timeout_sec = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
      copy_string(event_path, sizeof(event_path), argv[++i]);
    } else if (strcmp(argv[i], "--fb") == 0 && i + 1 < argc) {
      fb_path = argv[++i];
    } else if (strcmp(argv[i], "--egl-lib") == 0 && i + 1 < argc) {
      egl_path = argv[++i];
    } else if (strcmp(argv[i], "--gles-lib") == 0 && i + 1 < argc) {
      gles_path = argv[++i];
    } else if (strcmp(argv[i], "--rotation") == 0 && i + 1 < argc) {
      const char *rotation = argv[++i];
      if (strcmp(rotation, "auto") != 0 && strcmp(rotation, "none") != 0 &&
          strcmp(rotation, "cw") != 0 && strcmp(rotation, "ccw") != 0) {
        fprintf(stderr, "error: unknown rotation: %s\n", rotation);
        return 2;
      }
      copy_string(ui.mali_rotation, sizeof(ui.mali_rotation), rotation);
    } else if (strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
      mali_font_env = argv[++i];
    } else if (strcmp(argv[i], "--tty-entry-scale") == 0 && i + 1 < argc) {
      const char *scale = argv[++i];
      if (strcmp(scale, "1") != 0 && strcmp(scale, "1.0") != 0 &&
          strcmp(scale, "default") != 0 && strcmp(scale, "1.5") != 0 &&
          strcmp(scale, "2") != 0 && strcmp(scale, "2.0") != 0) {
        fprintf(stderr, "error: unknown TTY entry scale: %s\n", scale);
        return 2;
      }
      copy_string(ui.mali_tty_entry_scale, sizeof(ui.mali_tty_entry_scale), scale);
    } else if (strcmp(argv[i], "--rescue-network") == 0) {
      ui.rescue_network = 1;
      copy_string(ui.status, sizeof(ui.status), "Network Recovery is disabled");
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

  if (!ui.system_config_path[0] &&
      !join_path(ui.system_config_path, sizeof(ui.system_config_path), ui.plumos_root,
                 "config/system/settings.json")) {
    fprintf(stderr, "error: plumOS system settings path is too long\n");
    return 1;
  }

  if (!join_path(ui.top_cache_path, sizeof(ui.top_cache_path), ui.plumos_root,
                 "state/frontend/library-index.json") ||
      !join_path(ui.settings_path, sizeof(ui.settings_path), ui.plumos_root,
                 "config/frontend/settings.json") ||
      !join_path(ui.systems_path, sizeof(ui.systems_path), ui.plumos_root,
                 "config/frontend/systems.json") ||
      !join_path(ui.menus_path, sizeof(ui.menus_path), ui.plumos_root,
                 "config/frontend/menus.json") ||
      !join_path(ui.apps_path, sizeof(ui.apps_path), ui.plumos_root,
                 "config/frontend/apps.json") ||
      !join_path(ui.favorites_path, sizeof(ui.favorites_path), ui.plumos_root,
                 "state/frontend/favorites.json") ||
      !join_path(ui.recent_path, sizeof(ui.recent_path), ui.plumos_root,
                 "state/frontend/recent.json")) {
    fprintf(stderr, "error: frontend path is too long\n");
    return 1;
  }

  apply_frontend_cpu_default();
  if (load_device_settings(&ui)) {
    apply_device_runtime_settings(&ui.device, NULL, NULL, 0);
  }

  memset(&initial_settings, 0, sizeof(initial_settings));
  initial_settings_loaded = load_settings(ui.settings_path, &initial_settings);
  if (!initial_settings_loaded) {
    init_frontend_settings(&initial_settings);
  }
  ui.frontend_settings = initial_settings;
  load_theme_state(&ui, initial_settings_loaded ? initial_settings.graphic_theme_id : "default");
  apply_theme_setting_overrides(&ui.theme, &initial_settings);
  choose_mali_font_path(&ui, mali_font_env, ui.mali_font_path, sizeof(ui.mali_font_path));

  startup_resume_allowed =
      !ui.rescue_network && !script && !ui.once && initial_settings_loaded &&
      (!getenv("PLUMOS_FRONTEND_MODE") ||
       strcmp(getenv("PLUMOS_FRONTEND_MODE"), "manual") != 0);
  if (startup_resume_allowed) {
    run_boot_resume_if_needed(&ui, &initial_settings);
  }

  if (!ui.rescue_network && !load_top_entries(&ui)) {
    fprintf(stderr, "error: cannot load TOP entries: %s\n", ui.top_cache_path);
    return 1;
  }
  if (startup_resume_allowed && strcmp(initial_settings.boot_resume_mode, "picker") == 0) {
    open_recent_screen(&ui);
    set_status(&ui, "boot resume picker ready");
  }

#ifndef PLUMOS_ENABLE_MALI_RENDERER
  (void)fb_path;
  (void)egl_path;
  (void)gles_path;
#endif
  copy_string(ui.fb_path, sizeof(ui.fb_path),
              fb_path && fb_path[0] ? fb_path : "/dev/fb0");
  copy_string(ui.egl_path, sizeof(ui.egl_path),
              egl_path && egl_path[0] ? egl_path : "/usr/lib/libEGL.so");
  copy_string(ui.gles_path, sizeof(ui.gles_path),
              gles_path && gles_path[0] ? gles_path : "/usr/lib/libGLESv2.so");

  if (ui.renderer_mali) {
#ifdef PLUMOS_ENABLE_MALI_RENDERER
    if (!init_ui_renderer(&ui)) {
      fprintf(stderr, "error: %s\n", ui.status[0] ? ui.status : "Mali renderer init failed");
      return 1;
    }
    if (!ui.status[0] ||
        strncmp(ui.status, "Mali renderer ready font=", 25) == 0) {
      copy_string(ui.status, sizeof(ui.status), "Mali renderer ready");
    }
    if (ui.rescue_network) {
      copy_string(ui.status, sizeof(ui.status), "Network Recovery is disabled");
    }
#else
    fprintf(stderr, "error: this binary was built without Mali renderer support\n");
    return 2;
#endif
  }

  if (script) {
    exit_code = run_script(&ui, script) ? 0 : 1;
    if (ui.renderer_mali && ui.timeout_sec > 0) {
      sleep((unsigned int)ui.timeout_sec);
    }
  } else if (ui.once) {
    render_ui(&ui);
    if (ui.renderer_mali && ui.timeout_sec > 0) {
      sleep((unsigned int)ui.timeout_sec);
    }
    exit_code = ui.render_failed ? 1 : 0;
  } else {
    exit_code = run_event_loop(&ui, event_path);
  }
#ifdef PLUMOS_ENABLE_MALI_RENDERER
  if (ui.renderer_mali) {
    shutdown_ui_renderer(&ui);
  }
#endif
  return exit_code;
}
