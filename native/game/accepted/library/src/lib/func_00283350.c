#include "game/cutscene_audio_envelope.h"
#include "game/effect_presentation_p04.h"

typedef struct CutsceneAudioContext {
    u8 unk00[0x24];
    s32 handle;
} CutsceneAudioContext;

typedef struct CutscenePendingAudio {
    u8 unk00[0x1CB2];
    u8 requests[3];
} CutscenePendingAudio;

extern CutsceneAudioContext *table_00286878[];
extern void *D_8022A974;
extern s32 resource_free(u8 *memory);
extern void func_000118c4(s32 routeMask, s16 gain);
extern s32 func_00011f08(s32 handle);

void func_00283350(void) {
    /* Reusing the loop index's register for the unclamped gain matches retail. */
    s32 indexOrGain;
    CutsceneAudioContext **context;
    CutsceneAudioEnvelope *toFree;
    CutsceneAudioEnvelope *envelope;
    CutsceneAudioEnvelope *output;
    s16 remaining;
    s32 route;

    if (D_8022A974 != 0) {
        indexOrGain = 0;
        context = table_00286878;
        do {
            if (((CutscenePendingAudio *)D_8022A974)->requests[indexOrGain] != 0 &&
                func_00011f08((*context)->handle) == 0) {
                func_00208DC8(*context, ((CutscenePendingAudio *)D_8022A974)->requests[indexOrGain]);
                func_000118c4(2, 0x7FFF);
                toFree = D_8022A8C4;
                if (toFree != 0) {
                    resource_free((u8 *)toFree);
                    D_8022A8C4 = 0;
                    func_000118c4(1, 0x7FFF);
                }
                ((CutscenePendingAudio *)D_8022A974)->requests[indexOrGain] = 0;
            }
            indexOrGain++;
            context++;
        } while (indexOrGain < 3);
    }

    envelope = D_8022A8C4;
    if (envelope != 0) {
        remaining = envelope->remaining;
        if (remaining > 0) {
            indexOrGain = envelope->gain + envelope->step;
            envelope->remaining = remaining - 1;
            if (indexOrGain < 0) {
                envelope->gain = 0;
            } else if (indexOrGain > 0x7FFF) {
                envelope->gain = 0x7FFF;
            } else {
                envelope->gain = indexOrGain;
            }

            output = D_8022A8C4;
            route = output->route;
            if (route == 7) {
                func_000118c4(3, output->gain);
            } else if (route >= 2) {
                func_000118c4(1, output->gain);
            } else {
                func_000118c4(2, output->gain);
            }
        }
    }
}
