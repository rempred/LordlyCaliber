#include "game/effect_presentation_p06_internal.h"

#include "game/combat_draw.h"
#include "game/combat_draw_commands.h"

typedef union P06Matrix75EE0 { s32 words[16]; double alignment; } P06Matrix75EE0;
typedef struct P06Draw75EE0 { u8 unknown_00[8]; u16 value_08; } P06Draw75EE0;
typedef struct P06State75EE0 { u8 unknown_00[0x18]; P06Draw75EE0 draw; } P06State75EE0;

extern void guTranslateF(float [4][4], float, float, float);
extern void guMtxF2L(float [4][4], P06Matrix75EE0 *);
extern P06Matrix75EE0 *D_801969B0;
extern CombatDrawCommand D_80225620[];
s32 func_00275EE0(P06State75EE0 *state)
{
    float matrix[4][4];
    /* A label-address exit keeps the two return paths in retail order in the
     * pinned compiler. Direct conditional returns move the inactive return
     * ahead of the drawing body. The indirect exit folds to the same branch. */
    void *inactiveExit = &&disabled;
    P06Draw75EE0 *draw = &state->draw;

    if (!(draw->value_08 & 1)) {
        goto *inactiveExit;
    }
    {
        /* Keep the actual zero argument words while preserving the float ABI. */
        union { s32 bits; float value; } zero;
        zero.bits = 0;
        guTranslateF(matrix, zero.value, zero.value, zero.value);
        guMtxF2L(matrix, D_801969B0);
        COMBAT_DRAW_PAIR(0xDA380000, (u32)D_801969B0++);
        COMBAT_DRAW_PAIR(0xDE000000, (u32)D_80225620);
        func_00275AF8((struct P06State75AF8 *)(draw), func_00275850((struct P06State75850 *)(draw), 0));
        func_00275AF8((struct P06State75AF8 *)(draw), func_00275850((struct P06State75850 *)(draw), 1));
        COMBAT_DRAW_PAIR(0xD8380002, 0x40);
        return 0;
    }
disabled:
    return 0;
}
