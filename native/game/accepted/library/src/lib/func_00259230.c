#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

/* Numeric state prefix; the allocation extent remains unspecified. */
typedef struct P05TableRootState {
    u8 *context;
    u8 *values;
    u8 unknown_08[4];
    u8 value_0C;
    u8 unknown_0D;
    s16 index;
} P05TableRootState;


struct P05NineEntryState;
struct P05ScanState;
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F68;
extern P05FactoryRecord D_80209F1A[];
extern void func_00262B68(void);


s32 func_00259230(u16 message, P05TableRootState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)state = p01_context_80220F68;
        state->values = p01_pointer_80220F6C;
        state->value_0C = state->values[0];
        state->index = 0;
        func_0026C90C(state->context[0], 0x28);
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_00262C64(0x11, 0x1001);
        break;
    case 0x1001: {
        u8 value = state->context[0];

        p05_halfword_80220F6A = 1;
        p01_byte_80220F68 = value;
        func_00262AB0((void *)func_00258A28);
        func_00262C64(-1, 0x1002);
        break;
    }
    case 0x1002:
        p01_byte_80220F68 = state->value_0C;
        func_00262AB0((void *)func_002590C0);
        func_0024E9BC(0, state->context[0], 0xA2);
        func_00262830(0x1003);
        break;
    case 0x1003: {
        u8 value = state->values[state->index];

        if (value != 0xFF) {
            func_0024DA10(value, &D_80209F1A[0]);
            func_0025CAF0(value, 0xF, 0);
        }
        state->index++;
        if (state->values[state->index] == 0xFF) {
            func_0025CAF0(state->context[0], 0x1E, 0);
            func_00262C64(0x19, 0x1004);
        } else {
            func_00262C64(7, 0x1003);
        }
        break;
    }
    case 0x1004:
        func_0026C90C(state->context[0], 0x2A);
        func_00262C64(0x14, 0x1005);
        break;
    case 0x1005:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
