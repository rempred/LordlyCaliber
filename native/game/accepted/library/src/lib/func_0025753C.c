#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;

typedef struct P01PlacementObject {
    u8 unknown_00[0x30];
    P01Vec3 value_30;
} P01PlacementObject;


typedef struct P05CoordinateRootState {
    u8 *context;
    u8 unknown_04[8];
    u8 value_0C;
} P05CoordinateRootState;

struct P05CoordinateState;
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F68;
extern u8 p01_byte_80220F69;
/* Records remain within original data_0025e890, starting at ROM25EA02. */
extern P05FactoryRecord D_80209E42[];
extern void func_00262B68(void);
extern void func_0024C7A4(void *, void *, void *);




s32 func_0025753C(u16 message, P05CoordinateRootState *state)
{
    P01Vec3 difference;
    P01Vec3 from;
    P01Vec3 to;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Reuse the scalar first-store form of the P05 pointer views. */
        *(u8 **)state = p01_context_80220F68;
        state->value_0C = p01_pointer_80220F6C[0];
        func_00262830(0x1000);
        break;
    case 0x1000:
        p01_byte_80220F68 = state->context[0];
        p01_byte_80220F69 = state->value_0C;
        func_00262AB0((void *)func_00257124);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_0C, &D_80209E42[2]);
        func_0025CAF0(state->value_0C, 2, 5);
        func_0024E9BC(0, state->context[0], 0x95);
        func_00262C64(7, 0x1002);
        break;
    case 0x1002: {
        P01PlacementObject *object = func_0024CDD8(&D_80209E42[3]);

        func_0024BF04(state->context[0], &from, 0x10);
        func_0024BF04(state->value_0C, &to, 0x10);
        func_0024C7A4(&difference, &to, &from);
        func_0025EFC8(&difference);
        func_0024C7D8(&difference, object);
        object->value_30.x = to.x;
        object->value_30.y = to.y;
        object->value_30.z = to.z;
        func_00262C64(0x12, 0x1003);
        break;
    }
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
