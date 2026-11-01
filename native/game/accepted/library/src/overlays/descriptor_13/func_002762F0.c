#include "game/effect_presentation_p06_internal.h"

typedef struct P06State762F0 { u8 *value_00, *value_04; } P06State762F0;
typedef struct P06Object762F0 { u8 unknown_00[0x10]; s32 value_10; } P06Object762F0;

extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C;
extern u8 p06_data_80220F6D;
extern u8 p06_data_802256AC[], p06_data_802256B2[];
s32 func_002762F0(u16 message, P06State762F0 *state)
{
    float position[3];

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)state = p06_pointer_80220F68;
        state->value_04 = p06_pointer_80220F6C;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->value_00);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_00262830(0x1003);
        break;
    case 0x1003:
        func_0024C204(state->value_00, (struct P01Vec3 *)(position), 0x20);
        position[1] += 20.0f;
        ((P06Object762F0 *)func_0024D41C(position, p06_data_802256AC))->value_10 = 10;
        func_0026CCB4(position[0], position[1], position[2], 200);
        if (state->value_00[0] != state->value_04[0]) {
            func_00262C64(10, 0x1004);
        } else {
            func_00262C64(10, 0x1005);
        }
        break;
    case 0x1004:
        /* Scalar pointer views keep both handoffs and the subsequent reload
         * in the observed order across the shared context writes. */
        p06_pointer_80220F68 = *(u8 **)state;
        *(u8 *)&p06_pointer_80220F6C = (*(u8 **)((u8 *)state + 4))[0];
        func_0024E9BC(0, (*(u8 **)state)[0], 0x1D);
        p06_data_80220F6D = 0;
        func_00262AB0(func_00275FF0);
        func_00262C64(2, 0x1005);
        break;
    case 0x1005:
        func_0024E9BC(0, state->value_04[0], 0x1E);
        func_0024DA10(state->value_04[0], p06_data_802256B2);
        func_0025CAF0(state->value_04[0], 5, 25);
        func_00262C64(37, 0x1006);
        break;
    case 0x1006:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
