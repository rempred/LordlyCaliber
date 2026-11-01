#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P05Vec3CDB4 {
    float x, y, z;
} P05Vec3CDB4;


struct P05PairActor;
struct P05StateC8D0;

typedef struct P05EntryCDB4 {
    struct P05PairActor *actor;
    s16 value_04;
    u8 unknown_06[2];
    float value_08;
} P05EntryCDB4;

/* The five-entry numeric prefix places the counters; it is not a list bound. */
typedef struct P05StateCDB4 {
    u8 *values;
    P05Vec3CDB4 value_04;
    P05EntryCDB4 entries[5];
    s16 value_4C;
    s16 value_4E;
    s16 value_50;
} P05StateCDB4;

/* Command paths reuse this address for three distinct numeric views. */
typedef union P05WorkspaceCDB4 {
    P05Vec3CDB4 vector;
    u8 bytes[12];
    P05IntegerTriplet integers;
} P05WorkspaceCDB4;

extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F68;
extern u8 p01_byte_80220F69;
/* Original data_0025ed6c/float_0025edc0 retain these tables and doubles. */
extern s16 D_8020A1C8[];
extern const double D_8020A2B0, D_8020A2B8, D_8020A2C0;
extern void func_00262B68(void);



extern void *func_0020C478(unsigned int);
extern s32 func_0020C32C(void *);

extern float sinf(float);
extern float func_00020bf0(float);
extern float sqrtf(float);

static __inline__ s32 p05_angleCDB4(u8 base, s16 value)
{
    u16 j;

    for (j = 7; (s16)j >= 0; j--) {
        if (base + D_8020A1C8[(s16)j] == value) {
            return (s16)j * 45;
        }
    }
    return 0;
}

s32 func_0025CDB4(u16 message, P05StateCDB4 *state)
{
    P05WorkspaceCDB4 work;
    P05Vec3CDB4 base;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100: {
        u16 i;
        u8 value = 0x18;

        *(u8 **)state = p01_pointer_80220F6C;
        state->value_4C = state->value_50 = state->value_4E = 0;
        if (func_0024BCE8(state->values[0]) != 0) {
            value = 0x2A;
        }
        func_0024BF04(value, &state->value_04, 0);
        for (i = 0; state->values[(s16)i] != 0xFF; i++) {
            u8 current;

            state->entries[(s16)i].actor = func_0020C478(state->values[(s16)i]);
            state->entries[(s16)i].value_04 =
                p05_angleCDB4(value, func_0024C3EC(state->values[(s16)i]));
            current = state->values[(s16)i];
            func_0024BF04(value, &base, 0);
            func_0024BF04(current, &work.vector, 0);
            work.vector.x -= base.x;
            work.vector.z -= base.z;
            state->entries[(s16)i].value_08 =
                __builtin_sqrtf(work.vector.x * work.vector.x +
                                work.vector.z * work.vector.z);
        }
        func_00262830(0x1000);
        break;
    }
    case 0x1000:
        if (state->values[state->value_50] != 0xFF) {
            u8 current;

            work.bytes[0] = work.bytes[1] = (u8)state->value_4E + 0x26;
            current = state->values[state->value_50];
            if ((s16)func_0024BCA0(current) == 0) {
                u8 first = work.bytes[0];
                u8 second = work.bytes[1];

                p01_byte_80220F68 = current;
                p01_byte_80220F69 = first;
                p05_byte_80220F6A = second - first;
                func_00262AB0((void *)func_0025C8D0);
            }
            if (func_0020C32C(state->entries[state->value_50].actor) == 0) {
                func_0026C8AC((void **)state->entries[state->value_50].actor, 0x11);
            }
            state->value_4E--;
        }
        state->value_50++;
        if (state->values[state->value_50] == 0xFF) {
            func_00262830(0x1001);
        }
        break;
    case 0x1001: {
        u16 i;

        if (state->value_4C >= state->value_4E + 0x25) {
            for (i = 0; state->values[(s16)i] != 0xFF; i++) {
                if (func_0020C32C(state->entries[(s16)i].actor) == 0) {
                    func_0026C8AC((void **)state->entries[(s16)i].actor, 0x12);
                }
            }
            func_00262830(0x1002);
            break;
        }
        for (i = 0; state->values[(s16)i] != 0xFF; i++) {
            float x, z;

            x = state->entries[(s16)i].value_08 *
                sinf((float)((double)state->entries[(s16)i].value_04 /
                             D_8020A2B0 * D_8020A2B8));
            z = state->entries[(s16)i].value_08 *
                func_00020bf0((float)((double)state->entries[(s16)i].value_04 /
                                      D_8020A2B0 * D_8020A2B8));
            state->entries[(s16)i].value_08 -= D_8020A2C0;
            if (state->entries[(s16)i].value_08 < 0.0f) {
                state->entries[(s16)i].value_08 = 0.0f;
            }
            state->entries[(s16)i].value_04 += 15;
            if (state->entries[(s16)i].value_04 > 360) {
                state->entries[(s16)i].value_04 = 0;
            }
            work.integers.value_00 = state->value_04.x + x;
            work.integers.value_04 = state->value_04.y;
            work.integers.value_08 = state->value_04.z + z;
            func_0025CBA8(state->entries[(s16)i].actor, &work.integers);
        }
        state->value_4C++;
        break;
    }
    case 0x1002: {
        u16 i;

        for (i = 0; state->values[(s16)i] != 0xFF; i++) {
            func_0025CD38(state->entries[(s16)i].actor);
        }
        func_00262830(0x1003);
        break;
    }
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
