#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P01Vec3 { float x, y, z; } P01Vec3;
typedef struct P01PlacementObject {
    u8 unknown_00[0x30];
    P01Vec3 value_30;
} P01PlacementObject;

/* Refined numeric views of the existing P01 child prefix. */
typedef struct P01ChildObject {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
    u8 unknown_10[0xC];
    P01Vec3 value_1C;
    u8 unknown_28[0xC];
    float value_34;
} P01ChildObject;

/* This address view begins at offset0x18 in the copied object. */
typedef struct P05CopyView18 {
    float value_00[4][4];
    float value_40;
    s16 value_44;
    u16 value_46;
    u16 value_48;
} P05CopyView18;

typedef struct P05CopyObject58178 {
    u32 unknown_00;
    s32 value_04;
    u32 unknown_08;
    s32 value_0C;
    u8 unknown_10[8];
    P05CopyView18 value_18;
} P05CopyObject58178;

typedef struct P05ChoreographyState {
    u8 value_00;
    u8 unknown_01[3];
    u8 *values_04;
    P01Vec3 value_08;
    P01Vec3 value_14;
    float value_20;
    float value_24;
    float value_28;
    s16 value_2C;
    s16 value_2E;
    s16 value_30;
    u8 unknown_32[6];
    P05CopyObject58178 *object_38;
    P01ChildObject *object_3C;
} P05ChoreographyState;


extern u8 p01_byte_80220F68;
extern u8 *p01_pointer_80220F6C;
extern float p02_f32_80220F68[];
/* ROM25EAA4 and+6 remain in original data_0025e890. */
extern P05FactoryRecord D_80209EE4[];
extern void func_00262B68(void);
extern void func_00262C80(void);


extern void func_0024C7A4(void *, void *, void *);



extern void *func_00261DA8(void *);
extern void func_00261E30(void *);
extern void func_002623C4(void *);
extern void func_0026241C(void *, s32, s32);
struct P05CopyState;
extern void func_002101DC(void *, s32, s32,
                        s32 (*)(const void *, const void *));
extern void func_00022e90(float [4][4], float, float, float);
extern void guMtxCatF(float [4][4], float [4][4], float [4][4]);

/* Keep the hit-only vector workspace with its distance calculation. */
static __inline__ float p05_half_distance(P01Vec3 *origin, u8 index)
{
    P01Vec3 delta;
    P01Vec3 position;

    func_0024BF04(index, &position, 0x10);
    func_0024C7A4(&delta, &position, origin);
    return func_0025EF78(&delta) / 2.0f;
}

s32 func_00258178(u16 message, P05ChoreographyState *state)
{
    P01Vec3 point;
    P01Vec3 difference;
    P01Vec3 offset;
    float rotation[4][4];


    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100: {
        u8 firstIndex;
        u8 secondIndex;
        P01Vec3 *from;
        s16 i;
        s16 count;
        u8 *values;

        state->value_00 = p01_byte_80220F68;
        state->values_04 = p01_pointer_80220F6C;
        state->value_30 = func_0024BCE8(state->value_00);
        if (state->value_30 != 0) {
            offset.x = 0.0f;
            offset.y = 38.0f;
            offset.z = 15.0f;
            firstIndex = 0x3B;
            secondIndex = 0x3E;
        } else {
            offset.x = 5.0f;
            offset.y = 25.0f;
            offset.z = -10.0f;
            firstIndex = 0x4A;
            secondIndex = 0x47;
        }
        from = &state->value_08;
        func_0024C0DC(state->value_00, from, offset.x, offset.y, offset.z);
        func_0024BF04(firstIndex, &point, 0x10);
        func_0024C7A4(&difference, &point, from);
        state->value_20 = func_0024C8B4((float *)&difference);
        for (i = 0; i < 3; i++) {
            ((float *)&state->value_14)[i] = ((float *)&difference)[i];
        }
        func_0025EFC8(&state->value_14);
        func_0024BF04(secondIndex, &point, 0x10);
        func_0024C7A4(&difference, &point, &state->value_08);
        state->value_24 = func_0024C8B4((float *)&difference);
        state->value_2C = 0;
        state->value_2E = -1;
        state->value_28 = 1.0f;
        values = state->values_04;
        for (count = 0; values[count] != 0xFF; count++) {
        }
        func_002101DC(values, count, 1, func_002580DC);
        for (i = 0; state->values_04[i] != 0xFF; i++) {
            /* Observed byte-list view; its capacity is not established here. */
            ((u8 *)state + i)[0x32] = state->values_04[i];
        }
        func_00262830(0x1000);
        break;
    }
    case 0x1000: {
        P05CopyObject58178 *object = func_00261DA8((void *)func_002673C4);

        state->object_38 = object;
        func_0024C7D8(&state->value_14,
                    (P01PlacementObject *)object->value_18.value_00);
        object->value_18.value_00[3][0] = state->value_08.x;
        object->value_18.value_00[3][1] = state->value_08.y;
        object->value_18.value_00[3][2] = state->value_08.z;
        object->value_18.value_48 = 1;
        object->value_18.value_44 = 0;
        object->value_18.value_46 = 1;
        object->value_18.value_40 = 1.0f;
        state->object_3C = func_0024E510(&state->value_08, &D_80209EE4[0]);
        if (state->value_30 != 0) {
            state->object_3C->flags_0A |= 0x1000;
            state->object_3C->value_0C = -1000;
        }
        func_0026241C(state->object_3C, 1, 0xA);
        func_00262830(0x1001);
        break;
    }
    case 0x1001: {
        P05CopyObject58178 *object = state->object_38;

        if ((object->value_18.value_44 += 30) > 250) {
            object->value_18.value_44 = 250;
            func_00262C64(0xA, 0x1002);
        }
        break;
    }
    case 0x1002: {
        P05CopyObject58178 *object = state->object_38;
        P05CopyView18 *view = &object->value_18;
        s16 i;

        if (state->value_20 < state->value_24) {
            u16 previous;

            func_0025F058(view->value_00, p02_f32_80220F68);
            /* Scalar address views keep the snapshot copy sequence explicit. */
            p05_halfword_80220FA8 = *(u16 *)((u8 *)object + 0x5C);
            p05_halfword_80220FAA = *(u16 *)((u8 *)object + 0x5E);
            previous = (*(u16 *)((u8 *)state + 0x2C))++;
            p05_halfword_80220FAC = previous;
            if (*(s16 *)((u8 *)object + 0x5C) == 250 && state->value_30 == 0) {
                p05_halfword_80220FAC = previous + 10000;
            }
            func_00262AB0((void *)func_00257F54);
        }
        func_00022e90(rotation, 0.0f, state->value_28, 0.0f);
        guMtxCatF(rotation, view->value_00, view->value_00);
        state->value_20 += state->value_28;
        view->value_44 = 250;
        for (i = 0; state->values_04[i] != 0xFF; i++) {
            if (((u8 *)state + i)[0x32] != 0xFF) {
                s16 status = func_0024C964((float *)&state->value_08,
                                         state->values_04[i], state->value_20);

                if (status == 1) {
                    view->value_44 = (s16)(s32)p05_half_distance(
                        &state->value_08, state->values_04[i]);
                    if (i != state->value_2E) {
                        state->value_2E = i;
                        func_0024DA10(state->values_04[i], &D_80209EE4[1]);
                        func_0025CAF0(state->values_04[i], 0, 5);
                        func_0024E9BC(0, state->value_00, 0xA0);
                    }
                    break;
                } else if (status == 2) {
                    ((u8 *)state + i)[0x32] = 0xFF;
                }
            }
        }
        if (state->value_20 > state->value_24) {
            state->object_3C->flags_0A |= 0x2000;
            func_00262830(0x1003);
        }
        break;
    }
    case 0x1003: {
        P01ChildObject *secondary = state->object_3C;
        P05CopyObject58178 *object;
        float value;

        value = secondary->value_34 - 0.1f;
        object = state->object_38;
        secondary->value_34 = value;
        if ((object->value_18.value_44 -= 30) < 0) {
            object->value_18.value_44 = 0;
            secondary->value_34 = 0.0f;
            func_002623C4(state->object_3C);
            func_00261E30(state->object_38);
            func_00262830(0x1004);
        }
        break;
    }
    case 0x1004:
        func_0026C90C(state->value_00, 0x2A);
        func_00262C80();
        func_002628E4(0x101);
        break;
    }
    return 0;
}
