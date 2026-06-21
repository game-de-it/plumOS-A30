#define _GNU_SOURCE
#define MA_NO_DEVICE_IO
#define MA_NO_RESOURCE_MANAGER
#define MA_IMPLEMENTATION

#include "miniaudio.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<sys/soundcard.h>)
#include <sys/soundcard.h>
#define PLUMOS_MUSIC_HAS_OSS 1
#endif
#endif

#include "../frontend/plumos_mali_renderer.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MUSIC_MAX_TRACKS 2048
#define MUSIC_NAME_MAX 256
#define MUSIC_OUTPUT_RATE 44100
#define MUSIC_OUTPUT_CHANNELS 2
#define MUSIC_AUDIO_CHUNK_FRAMES 768
#define MUSIC_LIST_ROWS 10

struct music_track {
  char path[PATH_MAX];
  char name[MUSIC_NAME_MAX];
};

struct eq_preset {
  const char *name;
  float low;
  float mid;
  float high;
};

static const struct eq_preset k_eq_presets[] = {
    {"Flat", 1.00f, 1.00f, 1.00f},
    {"Bass", 1.35f, 0.98f, 0.94f},
    {"Treble", 0.92f, 1.00f, 1.28f},
    {"Vocal", 0.88f, 1.22f, 1.05f},
    {"Soft", 0.96f, 0.94f, 0.88f},
};

struct audio_filter {
  float low_state[MUSIC_OUTPUT_CHANNELS];
  float high_lp_state[MUSIC_OUTPUT_CHANNELS];
  int initialized;
};

struct player_state {
  struct music_track tracks[MUSIC_MAX_TRACKS];
  int track_count;
  int selected;
  int current;
  int scroll;

  pthread_mutex_t lock;
  pthread_t audio_thread;
  int audio_thread_started;
  volatile int running;

  ma_decoder decoder;
  int decoder_ready;
  int playing;
  int paused;
  ma_uint64 position_frames;
  ma_uint64 length_frames;
  int seek_delta_seconds;
  int eq_index;
  float volume;
  char status[192];
  char audio_status[128];
};

static long long monotonic_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static long long parse_env_ms(const char *name) {
  const char *value = getenv(name);
  char *end = NULL;
  long long parsed;
  if (!value || !value[0]) {
    return 0;
  }
  errno = 0;
  parsed = strtoll(value, &end, 10);
  if (errno != 0 || end == value || parsed < 0) {
    return 0;
  }
  return parsed;
}

static int str_ends_with_audio_ext(const char *name) {
  const char *dot = strrchr(name, '.');
  char ext[16];
  size_t i;
  if (!dot || dot[1] == '\0') {
    return 0;
  }
  dot++;
  for (i = 0; i + 1 < sizeof(ext) && dot[i]; i++) {
    ext[i] = (char)tolower((unsigned char)dot[i]);
  }
  ext[i] = '\0';
  return strcmp(ext, "mp3") == 0 || strcmp(ext, "flac") == 0 ||
         strcmp(ext, "wav") == 0;
}

static const char *base_name(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static void strip_audio_extension(const char *in, char *out, size_t out_size) {
  const char *name = base_name(in);
  const char *dot = strrchr(name, '.');
  size_t len;
  if (!out || out_size == 0) {
    return;
  }
  if (!dot || dot == name) {
    snprintf(out, out_size, "%s", name);
    return;
  }
  len = (size_t)(dot - name);
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, name, len);
  out[len] = '\0';
}

static int add_track(struct player_state *state, const char *path) {
  struct music_track *track;
  if (!state || !path || state->track_count >= MUSIC_MAX_TRACKS) {
    return 0;
  }
  track = &state->tracks[state->track_count++];
  snprintf(track->path, sizeof(track->path), "%s", path);
  strip_audio_extension(path, track->name, sizeof(track->name));
  return 1;
}

static void scan_dir_recursive(struct player_state *state, const char *dir, int depth) {
  DIR *d;
  struct dirent *ent;
  if (!state || !dir || depth > 8 || state->track_count >= MUSIC_MAX_TRACKS) {
    return;
  }
  d = opendir(dir);
  if (!d) {
    return;
  }
  while ((ent = readdir(d)) != NULL) {
    char path[PATH_MAX];
    struct stat st;
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0 ||
        ent->d_name[0] == '.') {
      continue;
    }
    if (snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name) >= (int)sizeof(path)) {
      continue;
    }
    if (stat(path, &st) != 0) {
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      scan_dir_recursive(state, path, depth + 1);
    } else if (S_ISREG(st.st_mode) && str_ends_with_audio_ext(ent->d_name)) {
      add_track(state, path);
    }
  }
  closedir(d);
}

static int track_compare(const void *a, const void *b) {
  const struct music_track *ta = (const struct music_track *)a;
  const struct music_track *tb = (const struct music_track *)b;
  int r = strcasecmp(ta->name, tb->name);
  if (r != 0) {
    return r;
  }
  return strcasecmp(ta->path, tb->path);
}

static void scan_music(struct player_state *state) {
  const char *roots[] = {
      "/mnt/SDCARD/Music",
      "/mnt/SDCARD/Roms/music",
      "/mnt/SDCARD/Roms/MUSIC",
  };
  size_t i;
  state->track_count = 0;
  for (i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
    scan_dir_recursive(state, roots[i], 0);
  }
  if (state->track_count > 1) {
    qsort(state->tracks, (size_t)state->track_count, sizeof(state->tracks[0]),
          track_compare);
  }
  state->selected = 0;
  state->current = -1;
  state->scroll = 0;
}

static void set_status_locked(struct player_state *state, const char *status) {
  snprintf(state->status, sizeof(state->status), "%s", status ? status : "");
}

static void set_audio_status_locked(struct player_state *state, const char *status) {
  snprintf(state->audio_status, sizeof(state->audio_status), "%s", status ? status : "");
}

static void unload_decoder_locked(struct player_state *state) {
  if (state->decoder_ready) {
    ma_decoder_uninit(&state->decoder);
    state->decoder_ready = 0;
  }
  state->playing = 0;
  state->paused = 0;
  state->position_frames = 0;
  state->length_frames = 0;
  state->seek_delta_seconds = 0;
}

static int load_track_locked(struct player_state *state, int index, const char *reason) {
  ma_decoder_config config;
  ma_result result;
  ma_uint64 length = 0;
  if (!state || index < 0 || index >= state->track_count) {
    return 0;
  }
  unload_decoder_locked(state);
  config = ma_decoder_config_init(ma_format_f32, MUSIC_OUTPUT_CHANNELS, MUSIC_OUTPUT_RATE);
  result = ma_decoder_init_file(state->tracks[index].path, &config, &state->decoder);
  if (result != MA_SUCCESS) {
    char msg[192];
    snprintf(msg, sizeof(msg), "Cannot play: %s", state->tracks[index].name);
    set_status_locked(state, msg);
    return 0;
  }
  if (ma_decoder_get_length_in_pcm_frames(&state->decoder, &length) != MA_SUCCESS) {
    length = 0;
  }
  state->decoder_ready = 1;
  state->current = index;
  state->selected = index;
  state->playing = 1;
  state->paused = 0;
  state->position_frames = 0;
  state->length_frames = length;
  state->seek_delta_seconds = 0;
  if (reason && reason[0]) {
    char msg[192];
    snprintf(msg, sizeof(msg), "%s: %s", reason, state->tracks[index].name);
    set_status_locked(state, msg);
  } else {
    set_status_locked(state, state->tracks[index].name);
  }
  return 1;
}

static void toggle_play_selected(struct player_state *state) {
  pthread_mutex_lock(&state->lock);
  if (state->decoder_ready && state->selected == state->current) {
    state->paused = !state->paused;
    set_status_locked(state, state->paused ? "Paused" : "Playing");
  } else if (state->track_count > 0) {
    load_track_locked(state, state->selected, "Playing");
  }
  pthread_mutex_unlock(&state->lock);
}

static void play_relative(struct player_state *state, int delta) {
  pthread_mutex_lock(&state->lock);
  if (state->track_count > 0) {
    int base = state->current >= 0 ? state->current : state->selected;
    int next = base + delta;
    if (next < 0) {
      next = state->track_count - 1;
    } else if (next >= state->track_count) {
      next = 0;
    }
    load_track_locked(state, next, "Playing");
  }
  pthread_mutex_unlock(&state->lock);
}

static void request_seek(struct player_state *state, int seconds) {
  pthread_mutex_lock(&state->lock);
  if (state->decoder_ready) {
    state->seek_delta_seconds += seconds;
    set_status_locked(state, seconds >= 0 ? "+5 seconds" : "-5 seconds");
  }
  pthread_mutex_unlock(&state->lock);
}

static void adjust_volume(struct player_state *state, float delta) {
  pthread_mutex_lock(&state->lock);
  state->volume += delta;
  if (state->volume < 0.0f) {
    state->volume = 0.0f;
  } else if (state->volume > 1.0f) {
    state->volume = 1.0f;
  }
  set_status_locked(state, "Volume changed");
  pthread_mutex_unlock(&state->lock);
}

static void cycle_eq(struct player_state *state) {
  pthread_mutex_lock(&state->lock);
  state->eq_index =
      (state->eq_index + 1) % (int)(sizeof(k_eq_presets) / sizeof(k_eq_presets[0]));
  set_status_locked(state, k_eq_presets[state->eq_index].name);
  pthread_mutex_unlock(&state->lock);
}

static int write_full(int fd, const void *data, size_t size) {
  const unsigned char *p = (const unsigned char *)data;
  size_t pos = 0;
  while (pos < size) {
    ssize_t n = write(fd, p + pos, size - pos);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return 0;
    }
    if (n == 0) {
      return 0;
    }
    pos += (size_t)n;
  }
  return 1;
}

static float clampf(float value, float lo, float hi) {
  if (value < lo) {
    return lo;
  }
  if (value > hi) {
    return hi;
  }
  return value;
}

static void reset_filter(struct audio_filter *filter) {
  memset(filter, 0, sizeof(*filter));
}

static void apply_eq_and_convert(const float *in, int16_t *out, ma_uint64 frames,
                                 const struct eq_preset *eq, float volume,
                                 struct audio_filter *filter) {
  const float low_alpha = 0.0277f;  /* roughly 200 Hz at 44.1 kHz */
  const float high_alpha = 0.3628f; /* roughly 4 kHz at 44.1 kHz */
  ma_uint64 i;
  int ch;
  if (!filter->initialized) {
    for (ch = 0; ch < MUSIC_OUTPUT_CHANNELS; ch++) {
      filter->low_state[ch] = 0.0f;
      filter->high_lp_state[ch] = 0.0f;
    }
    filter->initialized = 1;
  }
  for (i = 0; i < frames; i++) {
    for (ch = 0; ch < MUSIC_OUTPUT_CHANNELS; ch++) {
      size_t idx = (size_t)i * MUSIC_OUTPUT_CHANNELS + (size_t)ch;
      float x = in[idx];
      float low;
      float high_lp;
      float high;
      float mid;
      float y;
      filter->low_state[ch] += low_alpha * (x - filter->low_state[ch]);
      filter->high_lp_state[ch] += high_alpha * (x - filter->high_lp_state[ch]);
      low = filter->low_state[ch];
      high_lp = filter->high_lp_state[ch];
      high = x - high_lp;
      mid = x - low - high;
      y = (low * eq->low + mid * eq->mid + high * eq->high) * volume;
      y = clampf(y, -1.0f, 1.0f);
      out[idx] = (int16_t)lrintf(y * 32767.0f);
    }
  }
}

static void handle_seek_locked(struct player_state *state, struct audio_filter *filter) {
  ma_int64 target;
  ma_int64 delta;
  if (state->seek_delta_seconds == 0 || !state->decoder_ready) {
    return;
  }
  delta = (ma_int64)state->seek_delta_seconds * MUSIC_OUTPUT_RATE;
  target = (ma_int64)state->position_frames + delta;
  if (target < 0) {
    target = 0;
  }
  if (state->length_frames > 0 && (ma_uint64)target > state->length_frames) {
    target = (ma_int64)state->length_frames;
  }
  if (ma_decoder_seek_to_pcm_frame(&state->decoder, (ma_uint64)target) == MA_SUCCESS) {
    state->position_frames = (ma_uint64)target;
    reset_filter(filter);
  }
  state->seek_delta_seconds = 0;
}

static int open_configured_dsp(struct player_state *state) {
  int fd = open("/dev/dsp", O_WRONLY);
  if (fd < 0) {
    pthread_mutex_lock(&state->lock);
    set_audio_status_locked(state, "audio: /dev/dsp open failed");
    pthread_mutex_unlock(&state->lock);
    return -1;
  }
#if defined(PLUMOS_MUSIC_HAS_OSS)
  {
    int format = AFMT_S16_LE;
    int channels = MUSIC_OUTPUT_CHANNELS;
    int speed = MUSIC_OUTPUT_RATE;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) != 0 ||
        ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) != 0 ||
        ioctl(fd, SNDCTL_DSP_SPEED, &speed) != 0) {
      pthread_mutex_lock(&state->lock);
      set_audio_status_locked(state, "audio: OSS setup failed");
      pthread_mutex_unlock(&state->lock);
    } else {
      pthread_mutex_lock(&state->lock);
      set_audio_status_locked(state, "audio: /dev/dsp 44.1k stereo");
      pthread_mutex_unlock(&state->lock);
    }
  }
#else
  pthread_mutex_lock(&state->lock);
  set_audio_status_locked(state, "audio: OSS headers unavailable");
  pthread_mutex_unlock(&state->lock);
#endif
  return fd;
}

static void *audio_thread_main(void *arg) {
  struct player_state *state = (struct player_state *)arg;
  struct audio_filter filter;
  float pcm[MUSIC_AUDIO_CHUNK_FRAMES * MUSIC_OUTPUT_CHANNELS];
  int16_t out[MUSIC_AUDIO_CHUNK_FRAMES * MUSIC_OUTPUT_CHANNELS];
  int fd = open_configured_dsp(state);
  reset_filter(&filter);
  while (state->running) {
    ma_uint64 frames_read = 0;
    int should_sleep = 0;
    int ended = 0;
    int eq_index;
    float volume;

    pthread_mutex_lock(&state->lock);
    if (state->decoder_ready && state->playing && !state->paused) {
      handle_seek_locked(state, &filter);
      eq_index = state->eq_index;
      volume = state->volume;
      if (ma_decoder_read_pcm_frames(&state->decoder, pcm, MUSIC_AUDIO_CHUNK_FRAMES,
                                     &frames_read) == MA_SUCCESS &&
          frames_read > 0) {
        state->position_frames += frames_read;
      } else {
        ended = 1;
      }
    } else {
      should_sleep = 1;
      eq_index = state->eq_index;
      volume = state->volume;
    }

    if (ended) {
      int next = state->current + 1;
      if (next >= 0 && next < state->track_count) {
        load_track_locked(state, next, "Playing");
        reset_filter(&filter);
      } else {
        unload_decoder_locked(state);
        set_status_locked(state, "Finished");
      }
    }
    pthread_mutex_unlock(&state->lock);

    if (frames_read > 0 && fd >= 0) {
      apply_eq_and_convert(pcm, out, frames_read, &k_eq_presets[eq_index], volume,
                           &filter);
      if (!write_full(fd, out, (size_t)frames_read * MUSIC_OUTPUT_CHANNELS * sizeof(out[0]))) {
        close(fd);
        fd = -1;
        pthread_mutex_lock(&state->lock);
        set_audio_status_locked(state, "audio: write failed");
        pthread_mutex_unlock(&state->lock);
      }
    } else if (should_sleep || fd < 0) {
      usleep(10000);
      if (fd < 0) {
        fd = open_configured_dsp(state);
        if (fd < 0) {
          usleep(250000);
        }
      }
    }
  }
  if (fd >= 0) {
    close(fd);
  }
  return NULL;
}

static int read_trimmed_file(const char *path, char *out, size_t out_size) {
  int fd;
  ssize_t n;
  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    return 0;
  }
  n = read(fd, out, out_size - 1);
  close(fd);
  if (n <= 0) {
    return 0;
  }
  out[n] = '\0';
  while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) {
    out[--n] = '\0';
  }
  return 1;
}

static int find_gamepad_event(char *out, size_t out_size) {
  int i;
  for (i = 0; i < 32; i++) {
    char path[128];
    char name[128];
    snprintf(path, sizeof(path), "/sys/class/input/event%d/device/name", i);
    if (read_trimmed_file(path, name, sizeof(name)) &&
        strstr(name, "plumOS A30 Gamepad") != NULL) {
      snprintf(out, out_size, "/dev/input/event%d", i);
      return 1;
    }
  }
  return 0;
}

static void select_delta(struct player_state *state, int delta) {
  pthread_mutex_lock(&state->lock);
  if (state->track_count > 0) {
    state->selected += delta;
    if (state->selected < 0) {
      state->selected = state->track_count - 1;
    } else if (state->selected >= state->track_count) {
      state->selected = 0;
    }
    if (state->selected < state->scroll) {
      state->scroll = state->selected;
    } else if (state->selected >= state->scroll + MUSIC_LIST_ROWS) {
      state->scroll = state->selected - MUSIC_LIST_ROWS + 1;
    }
    if (state->scroll < 0) {
      state->scroll = 0;
    }
  }
  pthread_mutex_unlock(&state->lock);
}

static void handle_key_press(struct player_state *state, unsigned short code) {
  switch (code) {
  case BTN_A:
  case BTN_START:
    toggle_play_selected(state);
    break;
  case BTN_B:
  case BTN_MODE:
    state->running = 0;
    break;
  case BTN_X:
    play_relative(state, -1);
    break;
  case BTN_Y:
    play_relative(state, 1);
    break;
  case BTN_SELECT:
    cycle_eq(state);
    break;
  case BTN_TL:
    adjust_volume(state, -0.05f);
    break;
  case BTN_TR:
    adjust_volume(state, 0.05f);
    break;
  default:
    break;
  }
}

static void process_input_event(struct player_state *state, const struct input_event *ev) {
  if (!state || !ev) {
    return;
  }
  if (ev->type == EV_KEY) {
    if (ev->value == 1) {
      handle_key_press(state, ev->code);
    }
  } else if (ev->type == EV_ABS) {
    if (ev->code == ABS_HAT0Y) {
      if (ev->value < 0) {
        select_delta(state, -1);
      } else if (ev->value > 0) {
        select_delta(state, 1);
      }
    } else if (ev->code == ABS_HAT0X) {
      if (ev->value < 0) {
        request_seek(state, -5);
      } else if (ev->value > 0) {
        request_seek(state, 5);
      }
    }
  }
}

static void format_time(char *out, size_t out_size, ma_uint64 frames) {
  unsigned long seconds = (unsigned long)(frames / MUSIC_OUTPUT_RATE);
  unsigned long min = seconds / 60UL;
  unsigned long sec = seconds % 60UL;
  snprintf(out, out_size, "%lu:%02lu", min, sec);
}

static void draw_text_ft(struct plumos_mali_renderer *renderer, const char *text, float x,
                         float y, int scale, float max_x, float r, float g, float b,
                         float a) {
  plumos_mali_text_limited(renderer, text ? text : "", x, y, scale, 1, max_x, r, g, b,
                           a);
}

static void draw_progress(struct plumos_mali_renderer *renderer, float x, float y, float w,
                          float h, ma_uint64 pos, ma_uint64 len) {
  float ratio = 0.0f;
  plumos_mali_rect(renderer, x, y, w, h, 0.13f, 0.14f, 0.17f, 1.0f);
  if (len > 0 && pos <= len) {
    ratio = (float)((double)pos / (double)len);
  } else if (len > 0) {
    ratio = 1.0f;
  }
  if (ratio > 0.0f) {
    plumos_mali_rect(renderer, x, y, w * ratio, h, 0.95f, 0.48f, 0.10f, 1.0f);
  }
}

static void render_player(struct plumos_mali_renderer *renderer, struct player_state *state) {
  int track_count;
  int selected;
  int current;
  int scroll;
  int playing;
  int paused;
  int eq_index;
  int volume_percent;
  ma_uint64 pos;
  ma_uint64 len;
  char current_name[MUSIC_NAME_MAX];
  char status[192];
  char audio_status[128];
  char time_text[64];
  int i;

  pthread_mutex_lock(&state->lock);
  track_count = state->track_count;
  selected = state->selected;
  current = state->current;
  scroll = state->scroll;
  playing = state->playing;
  paused = state->paused;
  eq_index = state->eq_index;
  volume_percent = (int)lrintf(state->volume * 100.0f);
  pos = state->position_frames;
  len = state->length_frames;
  snprintf(current_name, sizeof(current_name), "%s",
           current >= 0 && current < state->track_count ? state->tracks[current].name : "-");
  snprintf(status, sizeof(status), "%s", state->status);
  snprintf(audio_status, sizeof(audio_status), "%s", state->audio_status);
  pthread_mutex_unlock(&state->lock);

  renderer->gl.Viewport(0, 0, renderer->fb_width, renderer->fb_height);
  renderer->gl.UseProgram(renderer->program);
  renderer->gl.ClearColor(0.030f, 0.034f, 0.042f, 1.0f);
  renderer->gl.Clear(GL_COLOR_BUFFER_BIT);

  plumos_mali_rect(renderer, 0.0f, 0.0f, (float)renderer->width, 44.0f, 0.06f, 0.07f,
                   0.09f, 1.0f);
  plumos_mali_rect(renderer, 0.0f, 44.0f, (float)renderer->width, 3.0f, 0.95f, 0.48f,
                   0.10f, 1.0f);
  draw_text_ft(renderer, "plumOS Music Player", 16.0f, 12.0f, 2,
               (float)renderer->width - 16.0f, 0.95f, 0.96f, 0.98f, 1.0f);

  if (track_count <= 0) {
    draw_text_ft(renderer, "No music found", 24.0f, 95.0f, 3,
                 (float)renderer->width - 24.0f, 0.95f, 0.96f, 0.98f, 1.0f);
    draw_text_ft(renderer, "Place MP3 / FLAC / WAV files in /mnt/SDCARD/Music", 24.0f,
                 150.0f, 2, (float)renderer->width - 24.0f, 0.70f, 0.75f, 0.82f,
                 1.0f);
  } else {
    float y = 62.0f;
    for (i = 0; i < MUSIC_LIST_ROWS; i++) {
      int idx = scroll + i;
      char line[MUSIC_NAME_MAX + 16];
      float row_y = y + (float)i * 34.0f;
      float bg_r = idx == selected ? 0.18f : 0.06f;
      float bg_g = idx == selected ? 0.18f : 0.07f;
      float bg_b = idx == selected ? 0.21f : 0.09f;
      if (idx >= track_count) {
        break;
      }
      plumos_mali_rect(renderer, 12.0f, row_y, (float)renderer->width - 24.0f, 29.0f,
                       bg_r, bg_g, bg_b, 1.0f);
      if (idx == current && playing && !paused) {
        snprintf(line, sizeof(line), "> %s", state->tracks[idx].name);
      } else if (idx == current && paused) {
        snprintf(line, sizeof(line), "|| %s", state->tracks[idx].name);
      } else {
        snprintf(line, sizeof(line), "  %s", state->tracks[idx].name);
      }
      draw_text_ft(renderer, line, 22.0f, row_y + 7.0f, 2,
                   (float)renderer->width - 22.0f, 0.90f, 0.92f, 0.95f, 1.0f);
    }
  }

  plumos_mali_rect(renderer, 0.0f, (float)renderer->height - 126.0f,
                   (float)renderer->width, 126.0f, 0.045f, 0.050f, 0.062f, 1.0f);
  draw_text_ft(renderer, current_name, 16.0f, (float)renderer->height - 112.0f, 2,
               (float)renderer->width - 16.0f, 1.0f, 1.0f, 1.0f, 1.0f);
  draw_progress(renderer, 16.0f, (float)renderer->height - 76.0f,
                (float)renderer->width - 32.0f, 10.0f, pos, len);
  {
    char t1[24];
    char t2[24];
    format_time(t1, sizeof(t1), pos);
    format_time(t2, sizeof(t2), len);
    snprintf(time_text, sizeof(time_text), "%s / %s   EQ:%s   VOL:%d%%", t1, t2,
             k_eq_presets[eq_index].name, volume_percent);
  }
  draw_text_ft(renderer, time_text, 16.0f, (float)renderer->height - 58.0f, 2,
               (float)renderer->width - 16.0f, 0.78f, 0.83f, 0.90f, 1.0f);
  draw_text_ft(renderer, status[0] ? status : audio_status, 16.0f,
               (float)renderer->height - 34.0f, 2, (float)renderer->width - 16.0f,
               0.95f, 0.65f, 0.28f, 1.0f);
  draw_text_ft(renderer,
               "A Play/Pause  B Exit  Left/Right 5s  X/Y Track  Select EQ  L/R Vol",
               16.0f, (float)renderer->height - 17.0f, 1,
               (float)renderer->width - 16.0f, 0.62f, 0.68f, 0.75f, 1.0f);

  renderer->gl.Finish();
  renderer->egl.SwapBuffers(renderer->display, renderer->surface);
}

static int init_renderer(struct plumos_mali_renderer *renderer) {
  char err[256];
  const char *rotation = getenv("PLUMOS_MALI_ROTATION");
  const char *font = getenv("PLUMOS_MUSIC_FONT");
  const char *fallback = getenv("PLUMOS_MUSIC_FALLBACK_FONT");
  if (!rotation || !rotation[0]) {
    rotation = "auto";
  }
  if (!font || !font[0]) {
    font = "/mnt/SDCARD/plumos/fonts/default.otf";
  }
  if (!fallback || !fallback[0]) {
    fallback = "/mnt/SDCARD/plumos/fonts/cjk-fallback.ttc";
  }
  if (!plumos_mali_renderer_init(renderer, "/dev/fb0", "/usr/lib/libEGL.so",
                                 "/usr/lib/libGLESv2.so", rotation, err,
                                 sizeof(err))) {
    fprintf(stderr, "renderer init failed: %s\n", err);
    return 0;
  }
  if (!plumos_mali_renderer_load_font(renderer, font, err, sizeof(err))) {
    fprintf(stderr, "font load failed: %s\n", err);
  }
  if (!plumos_mali_renderer_load_fallback_font(renderer, fallback, err, sizeof(err))) {
    fprintf(stderr, "fallback font load failed: %s\n", err);
  }
  return 1;
}

int main(void) {
  static struct player_state state;
  struct plumos_mali_renderer renderer;
  int input_fd = -1;
  long long last_input_scan = 0;
  long long start_ms = monotonic_ms();
  long long exit_after_ms = parse_env_ms("PLUMOS_MUSIC_EXIT_AFTER_MS");

  memset(&state, 0, sizeof(state));
  state.running = 1;
  state.current = -1;
  state.volume = 0.80f;
  pthread_mutex_init(&state.lock, NULL);
  scan_music(&state);
  snprintf(state.status, sizeof(state.status), "%d tracks", state.track_count);
  if (getenv("PLUMOS_MUSIC_AUTOPLAY") && state.track_count > 0) {
    pthread_mutex_lock(&state.lock);
    load_track_locked(&state, 0, "Playing");
    pthread_mutex_unlock(&state.lock);
  }

  if (!init_renderer(&renderer)) {
    pthread_mutex_destroy(&state.lock);
    return 1;
  }
  if (pthread_create(&state.audio_thread, NULL, audio_thread_main, &state) == 0) {
    state.audio_thread_started = 1;
  } else {
    pthread_mutex_lock(&state.lock);
    set_audio_status_locked(&state, "audio thread failed");
    pthread_mutex_unlock(&state.lock);
  }

  while (state.running) {
    struct pollfd pfd;
    if (exit_after_ms > 0 && monotonic_ms() - start_ms >= exit_after_ms) {
      state.running = 0;
      break;
    }
    if (input_fd < 0 && monotonic_ms() - last_input_scan > 1000) {
      char event_path[64];
      last_input_scan = monotonic_ms();
      if (find_gamepad_event(event_path, sizeof(event_path))) {
        input_fd = open(event_path, O_RDONLY | O_NONBLOCK);
      }
    }
    if (input_fd >= 0) {
      pfd.fd = input_fd;
      pfd.events = POLLIN;
      if (poll(&pfd, 1, 10) > 0 && (pfd.revents & POLLIN)) {
        struct input_event ev;
        while (read(input_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
          process_input_event(&state, &ev);
        }
      }
    } else {
      usleep(10000);
    }
    render_player(&renderer, &state);
    usleep(35000);
  }

  state.running = 0;
  if (state.audio_thread_started) {
    pthread_join(state.audio_thread, NULL);
  }
  pthread_mutex_lock(&state.lock);
  unload_decoder_locked(&state);
  pthread_mutex_unlock(&state.lock);
  if (input_fd >= 0) {
    close(input_fd);
  }
  plumos_mali_renderer_shutdown(&renderer);
  pthread_mutex_destroy(&state.lock);
  return 0;
}
