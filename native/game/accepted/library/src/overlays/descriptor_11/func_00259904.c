#include "game/effect_presentation_p06_internal.h"

typedef struct P06State59904 {
    u8 *value_00;
    u8 unknown_04[8];
    u8 value_0C;
} P06State59904;

struct P06State59480;
extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C;
extern u8 p06_data_80209F4A[];
s32 func_00259904(u16 message, P06State59904 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Scalar view retains the first-field store before the global load. */
        *(u8 **)state = p06_pointer_80220F68;
        state->value_0C = p06_pointer_80220F6C[0];
        func_00262830(0x1000);
        break;
    case 0x1000:
        p06_pointer_80220F68 = state->value_00;
        /* Initialization reads a pointer here; handoff writes its first byte. */
        *(u8 *)&p06_pointer_80220F6C = state->value_0C;
        func_00262AB0(func_00259480);
        func_0024E9BC(0, state->value_00[0], 0xA3);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001: {
        u8 *records = p06_data_80209F4A;

        func_0024DA10(state->value_0C, records);
        func_0024DA10(state->value_0C, records + 0xC);
        func_0024E9BC(0, state->value_00[0], 0xA4);
        func_00262830(0x1002);
        break;
    }
    case 0x1002:
        func_0024DA10(state->value_0C, p06_data_80209F4A + 6);
        func_0025CAF0(state->value_0C, 0x19, 0);
        func_00262C64(0x23, 0x1003);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
