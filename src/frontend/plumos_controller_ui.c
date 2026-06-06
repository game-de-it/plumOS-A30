#include <ctype.h>
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
#ifndef KEY_R
#define KEY_R 19
#endif
#ifndef KEY_LEFTCTRL
#define KEY_LEFTCTRL 29
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
#ifndef BTN_SELECT
#define BTN_SELECT 314
#endif
#ifndef BTN_START
#define BTN_START 315
#endif
#ifndef KEY_SELECT
#define KEY_SELECT 0x161
#endif
#ifndef KEY_MENU
#define KEY_MENU 139
#endif

#define UI_MAX_TOP 128
#define UI_MAX_ROMS 256
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
  char title[256];
  char relative_path[UI_PATH_MAX];
  char path[UI_PATH_MAX];
};

struct frontend_settings {
  int show_empty_systems;
  int show_favorites_on_top;
};

enum ui_screen {
  SCREEN_TOP = 0,
  SCREEN_ROMS = 1
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
  ACTION_QUIT
};

struct ui_state {
  const char *sdcard_root;
  char plumos_root[PATH_MAX];
  char top_cache_path[PATH_MAX];
  char settings_path[PATH_MAX];
  char favorites_path[PATH_MAX];
  int show_all;
  int refresh;
  int no_clear;
  int once;
  int timeout_sec;
  enum ui_screen screen;
  size_t top_cursor;
  size_t rom_cursor;
  struct top_entry top_entries[UI_MAX_TOP];
  size_t top_count;
  struct rom_entry rom_entries[UI_MAX_ROMS];
  size_t rom_count;
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
  free(json);
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
    fav.rom_count = count_json_array_objects(ui->favorites_path, "entries");
    fav.pinned = 1;
    fav.virtual_entry = 1;
    ui->top_entries[ui->top_count++] = fav;
  }
  if (ui->top_cursor >= ui->top_count) {
    ui->top_cursor = ui->top_count ? ui->top_count - 1 : 0;
  }
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
    copy_string(ui->status, sizeof(ui->status), "Favorites ROM view is not wired yet");
    return 1;
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
  printf("A: open  B: back  START: menu preview  SELECT: core preview  Q: quit\n");
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

  printf("plumOS controller UI - ROMS\n");
  printf("system=%s (%s)\n", ui->current_system_id, ui->current_system_name);
  printf("A: launch preview  B/LEFT: TOP  START: menu preview  SELECT: core preview  Q: quit\n");
  printf("entries=%zu cursor=%zu\n", ui->rom_count, ui->rom_count ? ui->rom_cursor + 1 : 0);
  printf("\n");
  for (i = start; i < end; i++) {
    const struct rom_entry *entry = &ui->rom_entries[i];
    printf("%c %3zu  %-34s %s\n",
           i == ui->rom_cursor ? '>' : ' ', i + 1, entry->title, entry->relative_path);
  }
  if (ui->rom_count == 0) {
    printf("(ROM entry is empty)\n");
  }
  if (ui->status[0]) {
    printf("\nstatus: %s\n", ui->status);
  }
}

static void render_ui(const struct ui_state *ui) {
  clear_screen(ui);
  if (ui->screen == SCREEN_ROMS) {
    render_roms(ui);
  } else {
    render_top(ui);
  }
  fflush(stdout);
}

static void set_status(struct ui_state *ui, const char *text) {
  copy_string(ui->status, sizeof(ui->status), text);
}

static void handle_action(struct ui_state *ui, enum ui_action action) {
  if (action == ACTION_NONE) {
    return;
  }
  if (action == ACTION_QUIT) {
    set_status(ui, "quit");
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
      copy_string(ui->current_system_id, sizeof(ui->current_system_id), entry->id);
      copy_string(ui->current_system_name, sizeof(ui->current_system_name), entry->display_name);
      ui->screen = SCREEN_ROMS;
      set_status(ui, "on-enter ROM scan");
      if (!load_rom_entries(ui, entry->id)) {
        set_status(ui, "cannot load ROM list");
      } else if (ui->rom_count == UI_MAX_ROMS) {
        set_status(ui, "ROM list truncated at prototype limit");
      } else {
        set_status(ui, "ROM list ready");
      }
      return;
    }
    if (action == ACTION_START) {
      set_status(ui, "START menu preview: settings/apps/favorites/recent/network");
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
    snprintf(msg, sizeof(msg), "launch preview: %s / %s", ui->current_system_id,
             entry->relative_path);
    set_status(ui, msg);
    return;
  }
  if (action == ACTION_START) {
    set_status(ui, "START menu preview from ROM list");
    return;
  }
  if (action == ACTION_SELECT && ui->rom_count > 0) {
    const struct rom_entry *entry = &ui->rom_entries[ui->rom_cursor];
    char msg[256];
    snprintf(msg, sizeof(msg), "per-ROM core preview: %s", entry->relative_path);
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
  case KEY_ENTER:
  case KEY_SPACE:
  case BTN_SOUTH:
  case KEY_Z:
  case 7:
    return ACTION_A;
  case KEY_ESC:
  case KEY_BACKSPACE:
  case BTN_EAST:
  case KEY_X:
  case 9:
    return ACTION_B;
  case KEY_MENU:
  case BTN_START:
  case KEY_RIGHTCTRL:
  case KEY_HOME:
  case 10:
    return ACTION_START;
  case KEY_SELECT:
  case BTN_SELECT:
  case KEY_TAB:
  case KEY_LEFTCTRL:
    return ACTION_SELECT;
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
  printf("Keyboard fallback over SSH: w/s/a/d, e or space for A, b, m, c, q.\n");
  printf("Environment:\n");
  printf("  PLUMOS_SDCARD_ROOT  Default: /mnt/SDCARD\n");
  printf("  PLUMOS_ROOT         Default: $PLUMOS_SDCARD_ROOT/plumos\n");
  printf("  PLUMOS_INPUT_EVENT  Default: auto-detect gpio-keys-polled\n");
}

int main(int argc, char **argv) {
  struct ui_state ui;
  const char *plumos_root_env;
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

  if (dump_events) {
    dump_input_events(event_path, ui.timeout_sec > 0 ? ui.timeout_sec : 10);
    return 0;
  }

  if (!join_path(ui.top_cache_path, sizeof(ui.top_cache_path), ui.plumos_root,
                 "state/frontend/library-index.json") ||
      !join_path(ui.settings_path, sizeof(ui.settings_path), ui.plumos_root,
                 "config/frontend/settings.json") ||
      !join_path(ui.favorites_path, sizeof(ui.favorites_path), ui.plumos_root,
                 "state/frontend/favorites.json")) {
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
