#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;

/* Numeric view of the fields used in these factory outputs. */
typedef struct P01ChildObject {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
    s32 value_10;
    u8 unknown_14[8];
    P01Vec3 value_1C;
    float value_28;
    float value_2C;
} P01ChildObject;


typedef struct P05NineEntry {
    P01ChildObject *object;
    u8 done;
    u8 unknown_05[3];
} P05NineEntry;

/* The observed nine-entry prefix does not define the allocation extent. */
typedef struct P05NineEntryState {
    u8 value_00;
    u8 unknown_01[3];
    P01Vec3 value_04;
    float value_10;
    s16 value_14;
    u8 unknown_16[2];
    P05NineEntry entries[9];
} P05NineEntryState;

extern u8 p01_byte_80220F68;
/* Numeric base for indices0x28/0x29; records remain in original data_0025e890. */
extern P05FactoryRecord D_80209E00[];
/* ROM25EE30/38/40 remain in the original float_0025edc0 owner. */
extern const double D_8020A270;
extern const double D_8020A278;
extern const double D_8020A280;
extern void func_00262B68(void);
extern void func_00262C80(void);



extern void func_0026241C(void *, s32, s32);
extern void func_00262434(void *);
extern void func_002623C4(void *);
extern u32 func_00022e60(void);
extern float sinf(float);
extern float func_00020bf0(float);

s32 func_00258A28(u16 message, P05NineEntryState *state)
{
    P01Vec3 offset;
    /* All paths carry commands 0x1000..0x1003 to the existing short provider. */
    s16 nextMessage;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_00 = p01_byte_80220F68;
        state->value_10 = 0.0f;
        state->value_14 = p05_halfword_80220F6A;
        if (func_0024BCE8(state->value_00) != 0) {
            offset.x = -15.0f;
            offset.y = 50.0f;
            offset.z = 5.0f;
        } else {
            offset.x = 18.0f;
            offset.y = 50.0f;
            offset.z = -5.0f;
        }
        func_0024C0DC(state->value_00, &state->value_04,
                    offset.x, offset.y, offset.z);
        nextMessage = 0x1000;
advance:
        /* The staged commands share this advancement call. */
        func_00262830(nextMessage);
        break;
    case 0x1000: {
        s32 recordIndex;
        s16 i;
        s32 randomValue;

        recordIndex = func_0024BCE8(state->value_00) != 0 ? 0x29 : 0x28;
        state->entries[0].object =
            func_0024D180(&state->value_04, &D_80209E00[recordIndex]);
        state->entries[0].done = 0;
        func_0026241C(state->entries[0].object, 3, 8);
        nextMessage = 0x1001;
        if (state->value_14 == 0) {
            goto advance;
        }
        for (i = 1; i < 9; i++) {
            state->entries[i].object =
                func_0024D180(&state->value_04, &D_80209E00[recordIndex]);
            state->entries[i].done = 0;
            randomValue = (s32)func_00022e60();
            state->entries[i].object->value_10 = randomValue % 8;
            func_0026241C(state->entries[i].object, 3, 8);
        }
        /* Read the mode again after the additional factory calls. */
        nextMessage = 0x1001;
        if (state->value_14 == 0) {
            goto advance;
        }
        nextMessage = 0x1002;
        goto advance;
    }
    case 0x1001: {
        P01ChildObject *object = state->entries[0].object;

        object->value_1C.y += 15.0f;
        /* Keep this order for the compiler's paired-update schedule. */
        object->value_28 += D_8020A270;
        object->value_2C += D_8020A270;
        if (object->value_1C.y > 300.0f) {
            func_00262434(state->entries[0].object);
            func_002623C4(state->entries[0].object);
            func_00262C80();
            nextMessage = 0x1003;
            goto advance;
        }
        break;
    }
    case 0x1002: {
        s16 i;
        s16 done = 0;
        float angle;
        float x;
        float z;
        P01ChildObject *object;

        for (i = 0; i < 9; i++) {
            if (state->entries[i].done != 0) {
                done++;
                continue;
            }
            angle = (i * 36 / D_8020A278) * D_8020A280;
            object = state->entries[i].object;
            x = state->value_10 * sinf(angle);
            z = state->value_10 * func_00020bf0(angle);
            object->value_1C.y += (s32)func_00022e60() % 10 + 5;
            object->value_1C.x = state->value_04.x + x;
            object->value_1C.z = state->value_04.z + z;
            if (object->value_1C.y > 250.0f) {
                func_00262434(state->entries[i].object);
                func_002623C4(state->entries[i].object);
                state->entries[i].done = 1;
            }
        }
        state->value_10 += 3.0f;
        /* Only entries already marked at loop entry contribute to this count. */
        if (done == 9) {
            func_00262C80();
            nextMessage = 0x1003;
            goto advance;
        }
        break;
    }
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
