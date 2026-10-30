#include "game/effect_callbacks_p02.h"

typedef struct P02RenderObject {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u8 unknown_0C[4];
    u32 value_10;
    u8 unknown_14[8];
    u8 value_1C[0x40];
} P02RenderObject;

typedef struct P02StateE520 {
    u8 unknown_00[0x40];
    P02RenderObject *value_40;
    u32 value_44;
    u32 unknown_48;
    u16 value_4C;
    u16 value_4E;
    u16 value_50;
    u16 value_52;
} P02StateE520;

extern u8 *p01_context_80220F68;
extern P02RenderObject *func_0026C010(s32);
extern void func_00262830(s32);
extern s32 func_002700B0(u16, void *);

s32 func_0026e520(u16 message, P02StateE520 *state)
{
    s32 result = 0;

    if (message == 0x100) {
        state->value_44 = (u32)p01_context_80220F68;
        state->value_40 = func_0026C010(0x37);
        state->value_40->value_10 = state->value_44;
        state->value_40->flags_0A |= 0x2000;
        state->value_4C = 0x20;
        state->value_4E = 0xC8;
        state->value_50 = 0xFF;
        state->value_52 = 0x3C;
        func_00262830(0x1000);
    } else {
        result = func_002700B0(message, state);
    }
    return result;
}
