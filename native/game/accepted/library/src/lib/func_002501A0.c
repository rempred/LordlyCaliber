#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04ShortState {
    u8 *context;
    u8 unknown_04[4];
    u8 value_08;
} P04ShortState;

extern void func_00262B68(void);
extern void func_0024E9BC(s32, u8, s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_80209846[];
extern u8 D_80209884[];
extern u8 D_8020988C[];

s32 func_002501A0(u16 message, P04ShortState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Retain the observed context store before the shared-pointer load. */
        *(u8 **)state = p01_context_80220F68;
        state->value_08 = *p01_pointer_80220F6C;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_08, D_80209846);
        func_0024E9BC(0, state->context[0], 0x37);
        func_0025CAF0(state->value_08, 0x1E, 0xF);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209884);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_8020988C);
        func_00262C64(0x31, 0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
