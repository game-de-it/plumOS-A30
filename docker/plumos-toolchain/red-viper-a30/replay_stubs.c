#include <stdbool.h>

#include "replay.h"
#include "vb_types.h"

void replay_init(void) {}
void replay_reset(bool has_sram) { (void)has_sram; }
void replay_update(HWORD inputs) { (void)inputs; }
void replay_save(char *fn) { (void)fn; }
void replay_load(char *fn) { (void)fn; }
bool replay_playing(void) { return false; }
HWORD replay_read(void) { return 0; }
