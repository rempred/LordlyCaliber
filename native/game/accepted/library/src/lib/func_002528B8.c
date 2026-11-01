#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"

typedef struct P03LoopState {
    u8 *context;
    u8 *values;
    u8 first;
    u8 unknown_09;
    s16 index;
} P03LoopState;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p03_data_80209A68[];
extern u8 p03_data_80209A94[];
extern u8 p03_data_80209A9C[];
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_002628E4(s32);
extern void func_00262C64(s16, s16);








s32 func_002528B8(u16 message, P03LoopState *state)
{
    u8 value;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* The first scalar pointer store preserves the observed access order. */
        *(u8 **)state = p01_context_80220F68;
        state->values = p01_pointer_80220F6C;
        state->first = state->values[0];
        state->index = 0;
        if ((s16)func_0024BCA0(state->first) != 0) {
            state->index++;
        }
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        func_0025DAB0(state->first, p03_data_80209A94);
        func_00262830(0x1002);
        /* fall through */
    case 0x1002:
        value = state->values[state->index];
        if (value != 0xFF) {
            func_0024E490(value, p03_data_80209A68, 3);
            func_0024E9BC(0, state->context[0], 0x61);
            func_0025CAF0(value, 0x1E, 0xA);
            func_0024E898(value, 0xFF);
        }
        state->index++;
        if (state->values[state->index] == 0xFF) {
            func_0025DAB0(value, p03_data_80209A9C);
            func_00262C64(0x35, 0x1003);
        } else {
            func_00262C64(0x14, 0x1002);
        }
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
