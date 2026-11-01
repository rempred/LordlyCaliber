#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"

typedef struct P01Vec3 { float x, y, z; } P01Vec3;
typedef struct P01PlacementObject {
    u8 unknown_00[0x30];
    P01Vec3 value_30;
} P01PlacementObject;
typedef struct P05StagedRootState {
    u8 *context;
    u8 unknown_04[8];
    u8 value_0C;
} P05StagedRootState;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern P05FactoryRecord D_80209F80[];
extern void func_00262B68(void);

extern void func_0024C7A4(void *, void *, void *);



s32 func_00259C44(u16 message, P05StagedRootState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)state = p01_context_80220F68;
        state->value_0C = p01_pointer_80220F6C[0];
        func_00262830(0x1000);
        break;
    case 0x1000: {
        P01Vec3 from;
        P01Vec3 to;
        P01Vec3 direction;
        P05FactoryRecord *record = &D_80209F80[3];
        P01PlacementObject *object = func_0024CDD8(record);

        func_0024BF04(state->context[0], &from, 0);
        func_0024BF04(state->value_0C, &to, 0);
        func_0024C7A4(&direction, &to, &from);
        func_0025EFC8(&direction);
        func_0024C7D8(&direction, object);
        object->value_30.x = from.x;
        object->value_30.y = from.y;
        object->value_30.z = from.z;
        func_0024E490(state->context[0], (const u8 *)(record - 3), 3);
        func_0024E9BC(0, state->context[0], 0xA6);
        func_00262C64(0x30, 0x1001);
        break;
    }
    case 0x1001: {
        P01Vec3 from;
        P01Vec3 to;
        P01Vec3 direction;
        P05FactoryRecord *record = &D_80209F80[4];
        P01PlacementObject *object = func_0024CDD8(record);

        func_0024BF04(state->context[0], &from, 0);
        func_0024BF04(state->value_0C, &to, 0);
        func_0024C7A4(&direction, &to, &from);
        func_0025EFC8(&direction);
        func_0024C7D8(&direction, object);
        object->value_30.x = to.x;
        object->value_30.y = to.y;
        object->value_30.z = to.z;
        func_0024DA10(state->value_0C, record + 1);
        func_0024E9BC(0, state->context[0], 0xA7);
        func_0025CAF0(state->value_0C, 0xA, 0xA);
        func_00262C64(7, 0x1002);
        break;
    }
    case 0x1002:
        func_0024DA10(state->value_0C, &D_80209F80[6]);
        func_00262C64(0x1F, 0x1003);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
