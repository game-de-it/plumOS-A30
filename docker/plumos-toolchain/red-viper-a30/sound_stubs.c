#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "v810_mem.h"
#include "vb_sound.h"

SOUND_STATE sound_state;
uint8_t sound_fill_buf;

void sound_init(void) {}
void sound_update(uint32_t cycles) { (void)cycles; }

void sound_write(int addr, uint16_t val) {
  if (vb_state && vb_state->V810_SOUND_RAM.pmemory) {
    vb_state->V810_SOUND_RAM.pmemory[addr & 0x7ff] = (uint8_t)val;
  }
}

void sound_refresh(void) {}
void sound_close(void) {}
void sound_pause(void) {}
void sound_resume(void) {}

void sound_reset(void) {
  memset(&sound_state, 0, sizeof(sound_state));
  sound_fill_buf = 0;
}

bool sound_init_backend(int16_t *wavebufs[]) {
  (void)wavebufs;
  return false;
}

void sound_close_backend(void) {}
void sound_pause_backend(void) {}
void sound_resume_backend(void) {}

bool sound_push_backend(int16_t *buf) {
  (void)buf;
  return true;
}
