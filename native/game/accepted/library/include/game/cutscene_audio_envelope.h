#ifndef OB64_GAME_CUTSCENE_AUDIO_ENVELOPE_H
#define OB64_GAME_CUTSCENE_AUDIO_ENVELOPE_H

#include "common/types.h"

typedef struct CutsceneAudioEnvelope {
    s16 step;
    s16 gain;
    s16 remaining;
    s16 route;
} CutsceneAudioEnvelope;

/* The pointer occupies the word at table_00286878 + 0x1C. */
extern CutsceneAudioEnvelope *D_8022A8C4;

void func_00283224(s32 route, s32 gainIndex, s32 step, s32 duration);
void func_00283350(void);

#endif
