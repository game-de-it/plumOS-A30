#include <alsa/asoundlib.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "vb_sound.h"

static snd_pcm_t *g_pcm;
static bool g_audio_ready;

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
    return 80000;
  }
  char *end = NULL;
  unsigned long parsed = strtoul(value, &end, 10);
  if (end == value || parsed < 10000 || parsed > 500000) {
    return 80000;
  }
  return (unsigned)parsed;
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

  rc = snd_pcm_set_params(g_pcm, SND_PCM_FORMAT_S16_LE,
                          SND_PCM_ACCESS_RW_INTERLEAVED, 2, SAMPLE_RATE, 1,
                          parse_latency_us());
  if (rc < 0) {
    fprintf(stderr, "red-viper-a30: ALSA configure %s failed: %s\n", device,
            snd_strerror(rc));
    snd_pcm_close(g_pcm);
    g_pcm = NULL;
    return true;
  }

  snd_pcm_prepare(g_pcm);
  g_audio_ready = true;
  return true;
}

void sound_close_backend(void) {
  if (!g_pcm) {
    return;
  }
  snd_pcm_drop(g_pcm);
  snd_pcm_close(g_pcm);
  g_pcm = NULL;
  g_audio_ready = false;
}

void sound_pause_backend(void) {
  if (g_pcm && g_audio_ready) {
    snd_pcm_drop(g_pcm);
  }
}

void sound_resume_backend(void) {
  if (g_pcm && g_audio_ready) {
    snd_pcm_prepare(g_pcm);
  }
}

bool sound_push_backend(int16_t *buf) {
  if (!g_pcm || !g_audio_ready) {
    return true;
  }

  int frames_left = SAMPLE_COUNT;
  int16_t *cursor = buf;
  while (frames_left > 0) {
    snd_pcm_sframes_t written = snd_pcm_writei(g_pcm, cursor, frames_left);
    if (written == -EPIPE || written == -ESTRPIPE) {
      snd_pcm_prepare(g_pcm);
      return true;
    }
    if (written < 0) {
      fprintf(stderr, "red-viper-a30: ALSA write failed: %s\n",
              snd_strerror((int)written));
      snd_pcm_prepare(g_pcm);
      return true;
    }
    if (written == 0) {
      return false;
    }
    cursor += written * 2;
    frames_left -= (int)written;
  }
  return true;
}
