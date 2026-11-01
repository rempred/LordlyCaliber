#include "game/effect_geometry_e05.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"

/* Accessed prefixes only; the remaining record extents are unresolved. */
typedef struct P05CopyObject57F54 {
    u32 unknown_00;
    s32 value_04;
    u32 unknown_08;
    s32 value_0C;
    u8 unknown_10[8];
    float value_18[16];
    float value_58;
    u16 value_5C;
    u16 value_5E;
    u16 value_60;
} P05CopyObject57F54;

typedef struct P05CopyState {
    float value_00[16];
    u16 value_40;
    u16 value_42;
    s16 value_44;
    u16 unknown_46;
    P05CopyObject57F54 *object_48;
} P05CopyState;

extern float p02_f32_80220F68[];
extern void func_00262B68(void);
extern void *func_00261DA8(void *);
extern void func_00261E30(void *);
s32 func_00257F54(u16 message, P05CopyState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        func_0025F058(p02_f32_80220F68, state->value_00);
        /* Scalar stores retain each write before the next shared load. */
        *(u16 *)((u8 *)state + 0x40) = p05_halfword_80220FA8;
        *(u16 *)((u8 *)state + 0x42) = p05_halfword_80220FAA;
        *(u16 *)((u8 *)state + 0x44) = p05_halfword_80220FAC;
        func_00262830(0x1000);
        break;
    case 0x1000: {
        P05CopyObject57F54 *object = func_00261DA8((void *)func_002673C4);

        state->object_48 = object;
        object->value_04 = object->value_0C = state->value_44;
        func_0025F058(state->value_00, object->value_18);
        object->value_60 = 1;
        object->value_58 = 1.0f;
        object->value_5C = state->value_40;
        object->value_5E = state->value_42;
        func_00262830(0x1001);
        break;
    }
    case 0x1001: {
        P05CopyObject57F54 *object = state->object_48;

        if ((object->value_58 -= 0.1f) < 0.0f) {
            object->value_58 = 0.0f;
            func_00262830(0x1002);
        }
        break;
    }
    case 0x1002:
        func_00261E30(state->object_48);
        func_002628E4(0x101);
        break;
    }
    return 0;
}

