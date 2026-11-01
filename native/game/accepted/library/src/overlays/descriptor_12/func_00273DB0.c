#include "game/effect_presentation_p06_internal.h"

typedef struct P06State73DB0 {
    u8 *value_00, *value_04, *value_08;
} P06State73DB0;

struct P06State56410;
extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C, *p06_pointer_80220F70;
extern u8 p06_byte_80220F68, p06_byte_80220F69;
extern u16 p06_half_80220F6A;
extern u8 D_800EB290[], D_800EB240[];
s32 func_00273DB0(u16 message, P06State73DB0 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Scalar pointer views retain the observed shared-context access order. */
        *(u8 **)state = p06_pointer_80220F68;
        *(u8 **)((u8 *)state + 4) = p06_pointer_80220F6C;
        state->value_08 = p06_pointer_80220F70;
        func_00262830(0x1000);
        break;
    case 0x1000:
        p06_byte_80220F68 = (*(u8 **)state)[0];
        p06_byte_80220F69 = (*(u8 **)((u8 *)state + 4))[0];
        p06_half_80220F6A = 1;
        func_00262AB0(func_00256410);
        func_0024E9BC((s32)D_800EB290, (*(u8 **)state)[0], 0x11B);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        func_0024E9BC((s32)D_800EB240, (*(u8 **)state)[0], 0x11C);
        func_00262C64(20, 0x101);
        break;
    }
    return 0;
}
