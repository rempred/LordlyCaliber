#include "game/effect_presentation_p06_internal.h"

#include "game/combat_draw.h"
#include "game/combat_draw_commands.h"

typedef struct P06State75AF8 {
    u32 value_00;
    void *value_04;
    u16 value_08;
    s16 value_0A;
    u8 unknown_0C[5];
    u8 value_11;
} P06State75AF8;
#define P06_PAIR COMBAT_DRAW_PAIR

void func_00275AF8(P06State75AF8 *state, CombatDrawVertex *vertices)
{
    s16 index;

    if (state->value_04 != 0) {
        P06_PAIR(0xFD180000, (u32)state->value_04);
        P06_PAIR(0xF5180000, 0x07000000);
        P06_PAIR(0xE6000000, 0);
        P06_PAIR(0xF3000000, 0x0717F100);
        P06_PAIR(0xE7000000, 0);
        P06_PAIR(0xF5180800, 0);
        P06_PAIR(0xF2000000, 0x0003C05C);
        P06_PAIR(0xFA000000, 0xFFFFFF00 | state->value_11);
        P06_PAIR(0xE6000000, 0);
        for (index = 0; index < state->value_0A * 4; index += 4) {
            P06_PAIR(0x01004008, (u32)&vertices[index]);
            P06_PAIR(0x06000204, 0x00000406);
        }
    }
}

#undef P06_PAIR
