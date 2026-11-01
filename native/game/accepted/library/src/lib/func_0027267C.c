#include "game/effect_commands_e09.h"
#include "game/effect_dispatch_e07.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02RenderObject {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u8 unknown_0C[4];
    u32 value_10;
    u8 unknown_14[8];
    u8 value_1C[0x40];
} P02RenderObject;

typedef struct P02State27267C {
    u8 unknown_00[0x40];
    P02RenderObject *value_40;
    u32 value_44;
    u32 unknown_48;
    u16 value_4C;
    u16 value_4E;
    u16 value_50;
    u16 value_52;
} P02State27267C;

extern u8 *p01_context_80220F68;

extern void func_00262830(s32);


s32 func_0027267C(u16 message, P02State27267C *state)
{
    s32 result = 0;

    if (message == 0x100) {
        state->value_44 = (u32)p01_context_80220F68;
        state->value_40 = func_0026C010(0x6E);
        state->value_40->value_10 = state->value_44;
        state->value_40->flags_0A |= 0x2000;
        state->value_4E = 0xFF;
        state->value_4C = 0;
        state->value_50 = 0x40;
        state->value_52 = 0x40;
        func_00262830(0x1000);
    } else {
        result = func_002700B0(message, state);
    }
    return result;
}
