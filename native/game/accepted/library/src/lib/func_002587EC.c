#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;

/* Only this returned-object prefix is used here. */
typedef struct P01ChildPrefix {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
} P01ChildPrefix;


/* Observed state prefix; no allocation extent is asserted. */
typedef struct P05ChoreographyRootState {
    u8 *context;
    u8 *values_04;
} P05ChoreographyRootState;

struct P05ChoreographyState;
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F68;
/* Two six-byte records at ROM25EA98/+6 remain in original data_0025e890. */
extern P05FactoryRecord D_80209ED8[];
extern void func_00262B68(void);





s32 func_002587EC(u16 message, P05ChoreographyRootState *state)
{
    P01Vec3 position;
    P01Vec3 offset;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Reuse the scalar first-store form of related root pointer views. */
        *(u8 **)state = p01_context_80220F68;
        state->values_04 = p01_pointer_80220F6C;
        func_0026C90C(state->context[0], 0x29);
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024DA10(state->context[0], &D_80209ED8[0]);
        func_0024E9BC(0, state->context[0], 0x9F);
        func_00262C64(0x23, 0x1001);
        break;
    case 0x1001: {
        P01ChildPrefix *object;

        if (func_0024BCE8(state->context[0]) != 0) {
            offset.x = -5.0f;
            offset.y = 22.0f;
            offset.z = 8.0f;
        } else {
            offset.x = 5.0f;
            offset.y = 20.0f;
            offset.z = -10.0f;
        }
        func_0024C0DC(state->context[0], &position,
                    offset.x, offset.y, offset.z);
        object = func_0024D180(&position, &D_80209ED8[1]);
        if (func_0024BCE8(state->context[0]) != 0) {
            object->value_0C = -1000;
            object->flags_0A |= 0x1000;
        }
        func_00262C64(0xA, 0x1002);
        break;
    }
    case 0x1002:
        p01_byte_80220F68 = state->context[0];
        /* Scalar view keeps this read after the shared byte store. */
        p01_pointer_80220F6C = *(u8 **)((u8 *)state + 0x4);
        func_00262AB0((void *)func_00258178);
        func_00262C64(-1, 0x1003);
        break;
    case 0x1003:
        func_00262830(0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
