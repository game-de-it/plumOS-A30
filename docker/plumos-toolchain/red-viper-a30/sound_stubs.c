#include <alsa/asoundlib.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "vb_sound.h"

static snd_pcm_t *g_pcm;
static bool g_audio_ready;
static unsigned g_prebuffer_chunks = 6;
static unsigned g_queue_chunks = 8;
static int16_t *g_audio_queue;
static unsigned g_queue_read;
static unsigned g_queue_write;
static unsigned g_queue_count;
static int16_t g_last_chunk[SAMPLE_COUNT * 2];
typedef enum {
  AUDIO_GAP_FADE,
  AUDIO_GAP_SILENCE,
  AUDIO_GAP_REPEAT,
} audio_gap_mode_t;
static audio_gap_mode_t g_audio_gap_mode = AUDIO_GAP_FADE;
static pthread_t g_audio_thread;
static pthread_mutex_t g_audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_audio_thread_started;
static bool g_audio_thread_stop;
static unsigned long g_audio_repeat_count;
static unsigned long g_audio_drop_count;
static unsigned long g_audio_xrun_count;

static bool audio_disabled_env(void) {
  const char *value = getenv("PLUMOS_A30_RED_VIPER_AUDIO");
  if (!value || !*value) {
    return false;
  }
  return strcasecmp(value, "0") == 0 || strcasecmp(value, "off") == 0 ||
         strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0 ||
         strcasecmp(value, "none") == 0 || strcasecmp(value, "disabled") == 0;
}

static unsigned parse_latency_us(void) {
  const char *value = getenv("PLUMOS_A30_RED_VIPER_AUDIO_LATENCY_US");
  if (!value || !*value) {
    return 160000;
  }
  char *end = NULL;
  unsigned long parsed = strtoul(value, &end, 10);
  if (end == value || parsed < 120000 || parsed > 500000) {
    return 160000;
  }
  return (unsigned)parsed;
}

static unsigned parse_prebuffer_chunks(void) {
  const char *value = getenv("PLUMOS_A30_RED_VIPER_AUDIO_PREBUFFER_CHUNKS");
  if (!value || !*value) {
    return 6;
  }
  char *end = NULL;
  unsigned long parsed = strtoul(value, &end, 10);
  if (end == value || parsed > BUF_COUNT - 1) {
    return 6;
  }
  return (unsigned)parsed;
}

static unsigned parse_queue_chunks(void) {
  const char *value = getenv("PLUMOS_A30_RED_VIPER_AUDIO_QUEUE_CHUNKS");
  if (!value || !*value) {
    return 8;
  }
  char *end = NULL;
  unsigned long parsed = strtoul(value, &end, 10);
  if (end == value || parsed < 2 || parsed > 32) {
    return 8;
  }
  return (unsigned)parsed;
}

static audio_gap_mode_t parse_audio_gap_mode(const char *value) {
  if (!value || !*value) {
    return AUDIO_GAP_FADE;
  }
  if (strcasecmp(value, "repeat") == 0 || strcasecmp(value, "hold") == 0) {
    return AUDIO_GAP_REPEAT;
  }
  if (strcasecmp(value, "silence") == 0 || strcasecmp(value, "silent") == 0 ||
      strcasecmp(value, "zero") == 0 || strcasecmp(value, "mute") == 0) {
    return AUDIO_GAP_SILENCE;
  }
  return AUDIO_GAP_FADE;
}

static const char *audio_gap_mode_name(audio_gap_mode_t mode) {
  switch (mode) {
  case AUDIO_GAP_REPEAT:
    return "repeat";
  case AUDIO_GAP_SILENCE:
    return "silence";
  case AUDIO_GAP_FADE:
  default:
    return "fade";
  }
}

void sound_backend_set_gap_mode(const char *value) {
  pthread_mutex_lock(&g_audio_mutex);
  g_audio_gap_mode = parse_audio_gap_mode(value);
  pthread_mutex_unlock(&g_audio_mutex);
}

const char *sound_backend_gap_mode_name(void) {
  const char *name;
  pthread_mutex_lock(&g_audio_mutex);
  name = audio_gap_mode_name(g_audio_gap_mode);
  pthread_mutex_unlock(&g_audio_mutex);
  return name;
}

void sound_backend_get_counters(unsigned long *repeats, unsigned long *drops,
                                unsigned long *xruns) {
  pthread_mutex_lock(&g_audio_mutex);
  if (repeats) {
    *repeats = g_audio_repeat_count;
  }
  if (drops) {
    *drops = g_audio_drop_count;
  }
  if (xruns) {
    *xruns = g_audio_xrun_count;
  }
  pthread_mutex_unlock(&g_audio_mutex);
}

static void tune_sw_params(void) {
  snd_pcm_sw_params_t *sw = NULL;
  snd_pcm_sw_params_alloca(&sw);
  if (snd_pcm_sw_params_current(g_pcm, sw) < 0) {
    return;
  }
  snd_pcm_sw_params_set_avail_min(g_pcm, sw, SAMPLE_COUNT);
  snd_pcm_sw_params_set_start_threshold(g_pcm, sw,
                                        SAMPLE_COUNT * g_prebuffer_chunks);
  snd_pcm_sw_params(g_pcm, sw);
}

static int write_frames_blocking(const int16_t *buf, int frames) {
  const int16_t *cursor = buf;
  int frames_left = frames;
  while (frames_left > 0) {
    snd_pcm_sframes_t written = snd_pcm_writei(g_pcm, cursor, frames_left);
    if (written == -EINTR) {
      continue;
    }
    if (written == -EPIPE || written == -ESTRPIPE) {
      snd_pcm_prepare(g_pcm);
      return -1;
    }
    if (written < 0) {
      fprintf(stderr, "red-viper-a30: ALSA write failed: %s\n",
              snd_strerror((int)written));
      snd_pcm_prepare(g_pcm);
      return -1;
    }
    if (written == 0) {
      return 0;
    }
    cursor += written * 2;
    frames_left -= (int)written;
  }
  return 0;
}

static int16_t *queue_slot(unsigned index) {
  return g_audio_queue + (size_t)index * SAMPLE_COUNT * 2;
}

static void queue_reset(bool clear_last_chunk) {
  pthread_mutex_lock(&g_audio_mutex);
  g_queue_read = 0;
  g_queue_write = 0;
  g_queue_count = 0;
  if (clear_last_chunk) {
    memset(g_last_chunk, 0, sizeof(g_last_chunk));
  }
  pthread_mutex_unlock(&g_audio_mutex);
}

static void fill_gap_chunk_locked(int16_t *chunk) {
  switch (g_audio_gap_mode) {
  case AUDIO_GAP_REPEAT:
    memcpy(chunk, g_last_chunk, sizeof(g_last_chunk));
    break;
  case AUDIO_GAP_SILENCE:
    memset(chunk, 0, sizeof(g_last_chunk));
    memset(g_last_chunk, 0, sizeof(g_last_chunk));
    break;
  case AUDIO_GAP_FADE:
  default:
    for (int i = 0; i < SAMPLE_COUNT; i++) {
      int remaining = SAMPLE_COUNT - i;
      chunk[i * 2] = (int16_t)((int32_t)g_last_chunk[i * 2] * remaining /
                               SAMPLE_COUNT);
      chunk[i * 2 + 1] =
          (int16_t)((int32_t)g_last_chunk[i * 2 + 1] * remaining /
                    SAMPLE_COUNT);
    }
    memset(g_last_chunk, 0, sizeof(g_last_chunk));
    break;
  }
}

static void *audio_thread_main(void *arg) {
  (void)arg;
  int16_t chunk[SAMPLE_COUNT * 2];

  for (;;) {
    pthread_mutex_lock(&g_audio_mutex);
    if (g_audio_thread_stop) {
      pthread_mutex_unlock(&g_audio_mutex);
      break;
    }

    if (g_queue_count > 0) {
      memcpy(chunk, queue_slot(g_queue_read), sizeof(chunk));
      g_queue_read = (g_queue_read + 1) % g_queue_chunks;
      g_queue_count--;
      memcpy(g_last_chunk, chunk, sizeof(g_last_chunk));
    } else {
      fill_gap_chunk_locked(chunk);
      g_audio_repeat_count++;
    }
    pthread_mutex_unlock(&g_audio_mutex);

    if (write_frames_blocking(chunk, SAMPLE_COUNT) < 0) {
      pthread_mutex_lock(&g_audio_mutex);
      g_audio_xrun_count++;
      memset(g_last_chunk, 0, sizeof(g_last_chunk));
      pthread_mutex_unlock(&g_audio_mutex);
    }
  }

  return NULL;
}

static bool start_audio_thread(void) {
  if (g_audio_thread_started) {
    return true;
  }

  g_audio_thread_stop = false;
  int rc = pthread_create(&g_audio_thread, NULL, audio_thread_main, NULL);
  if (rc != 0) {
    fprintf(stderr, "red-viper-a30: audio thread start failed: %s\n",
            strerror(rc));
    return false;
  }
  g_audio_thread_started = true;
  return true;
}

static void stop_audio_thread(void) {
  if (!g_audio_thread_started) {
    return;
  }

  pthread_mutex_lock(&g_audio_mutex);
  g_audio_thread_stop = true;
  pthread_mutex_unlock(&g_audio_mutex);
  pthread_join(g_audio_thread, NULL);
  g_audio_thread_started = false;
}

static void prebuffer_silence(void) {
  if (!g_pcm || !g_audio_ready || g_prebuffer_chunks == 0) {
    return;
  }

  int16_t silence[SAMPLE_COUNT * 2];
  memset(silence, 0, sizeof(silence));
  for (unsigned i = 0; i < g_prebuffer_chunks; i++) {
    if (write_frames_blocking(silence, SAMPLE_COUNT) < 0) {
      break;
    }
  }
}

bool sound_init_backend(int16_t *wavebufs[]) {
  (void)wavebufs;

  if (audio_disabled_env()) {
    return true;
  }

  const char *device = getenv("PLUMOS_A30_RED_VIPER_ALSA_DEVICE");
  if (!device || !*device) {
    device = "default";
  }

  int rc = snd_pcm_open(&g_pcm, device, SND_PCM_STREAM_PLAYBACK, 0);
  if (rc < 0) {
    fprintf(stderr, "red-viper-a30: ALSA open %s failed: %s\n", device,
            snd_strerror(rc));
    g_pcm = NULL;
    return true;
  }

  g_prebuffer_chunks = parse_prebuffer_chunks();
  g_queue_chunks = parse_queue_chunks();
  g_audio_gap_mode = parse_audio_gap_mode(getenv("PLUMOS_A30_RED_VIPER_AUDIO_GAP"));
  g_audio_repeat_count = 0;
  g_audio_drop_count = 0;
  g_audio_xrun_count = 0;
  g_audio_queue =
      calloc((size_t)g_queue_chunks * SAMPLE_COUNT * 2, sizeof(*g_audio_queue));
  if (!g_audio_queue) {
    fprintf(stderr, "red-viper-a30: audio queue allocation failed\n");
    snd_pcm_close(g_pcm);
    g_pcm = NULL;
    return true;
  }
  queue_reset(true);

  rc = snd_pcm_set_params(g_pcm, SND_PCM_FORMAT_S16_LE,
                          SND_PCM_ACCESS_RW_INTERLEAVED, 2, SAMPLE_RATE, 1,
                          parse_latency_us());
  if (rc < 0) {
    fprintf(stderr, "red-viper-a30: ALSA configure %s failed: %s\n", device,
            snd_strerror(rc));
    snd_pcm_close(g_pcm);
    g_pcm = NULL;
    free(g_audio_queue);
    g_audio_queue = NULL;
    return true;
  }

  tune_sw_params();
  snd_pcm_prepare(g_pcm);
  g_audio_ready = true;
  prebuffer_silence();
  if (!start_audio_thread()) {
    free(g_audio_queue);
    g_audio_queue = NULL;
  }
  return true;
}

void sound_close_backend(void) {
  if (!g_pcm) {
    return;
  }
  stop_audio_thread();
  snd_pcm_drop(g_pcm);
  snd_pcm_close(g_pcm);
  g_pcm = NULL;
  g_audio_ready = false;
  free(g_audio_queue);
  g_audio_queue = NULL;
  fprintf(stderr,
          "red-viper-a30: audio stats repeats=%lu drops=%lu xruns=%lu\n",
          g_audio_repeat_count, g_audio_drop_count, g_audio_xrun_count);
}

void sound_pause_backend(void) {
  if (g_pcm && g_audio_ready) {
    stop_audio_thread();
    snd_pcm_drop(g_pcm);
    queue_reset(true);
  }
}

void sound_resume_backend(void) {
  if (g_pcm && g_audio_ready) {
    snd_pcm_prepare(g_pcm);
    queue_reset(true);
    prebuffer_silence();
    start_audio_thread();
  }
}

bool sound_push_backend(int16_t *buf) {
  if (!g_pcm || !g_audio_ready) {
    return true;
  }

  if (!g_audio_thread_started || !g_audio_queue) {
    if (write_frames_blocking(buf, SAMPLE_COUNT) < 0) {
      prebuffer_silence();
    }
    return true;
  }

  pthread_mutex_lock(&g_audio_mutex);
  if (g_queue_count == g_queue_chunks) {
    g_queue_read = (g_queue_read + 1) % g_queue_chunks;
    g_queue_count--;
    g_audio_drop_count++;
  }
  memcpy(queue_slot(g_queue_write), buf, (size_t)SAMPLE_COUNT * 2 * sizeof(*buf));
  g_queue_write = (g_queue_write + 1) % g_queue_chunks;
  g_queue_count++;
  pthread_mutex_unlock(&g_audio_mutex);
  return true;
}
