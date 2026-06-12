#include <string.h>

#include "v810_mem.h"
#include "vb_dsp.h"

VB_DSPCACHE tDSPCACHE;
int eye_count = 2;
bool is_citra = false;

void video_init(void) {
  setup_brightness_lut();
  clearCache();
}

void video_download_vip(int drawn_fb) { (void)drawn_fb; }

void video_render(int displayed_fb, bool on_time) {
  (void)displayed_fb;
  (void)on_time;
}

void video_flush(bool default_for_both) { (void)default_for_both; }
void video_quit(void) {}

void video_hard_render(int drawn_fb) {
  video_soft_render(drawn_fb);
}

void input_init(void) {}

HWORD V810_RControll(bool reset) {
  (void)reset;
  if (!vb_state) {
    return 0;
  }
  return ((HWORD)vb_state->tHReg.SHB << 8) | vb_state->tHReg.SLB;
}
