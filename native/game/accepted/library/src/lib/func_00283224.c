#include "game/cutscene_audio_envelope.h"

typedef struct AudioGainEntry {
    s32 gain;
    u8 unk04[4];
} AudioGainEntry;

extern AudioGainEntry D_800EB2E0[];
extern void *func_00070F30(s32 size);
extern void bzero(void *memory, s32 size);

void func_00283224(s32 route, s32 gainIndex, s32 step, s32 duration) {
    s32 halfStep = step / 2;

    if (D_8022A8C4 == 0) {
        /* Retail passes 4 to both calls before writing the four halfwords. */
        D_8022A8C4 = func_00070F30(4);
        bzero(D_8022A8C4, 4);
        if ((u32)route - 2U < 5U) {
            D_8022A8C4->gain = 0x7FFF;
        } else {
            D_8022A8C4->gain = D_800EB2E0[gainIndex].gain;
        }
        if (step > 0) {
            /* The gain adjustment wraps before being stored in a halfword. */
            D_8022A8C4->gain =
                (u16)D_8022A8C4->gain - (u32)halfStep * (u32)duration;
        }
    }
    D_8022A8C4->step = halfStep;
    D_8022A8C4->remaining = duration;
    D_8022A8C4->route = route;
}
