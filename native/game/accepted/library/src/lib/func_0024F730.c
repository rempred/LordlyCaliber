#include "game/effect_math_e04.h"
#include "common/types.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

typedef struct P01SixByteRecord {
    u8 bytes[6];
} P01SixByteRecord;

typedef struct P01F730State {
    u8 *context;
    u8 value_04;
    u8 unknown_05[3];
    P01Vec3 value_08;
    P01Vec3 value_14;
    float value_20;
    u8 value_24;
    u8 unknown_25;
    s16 value_26;
    s16 value_28;
    s16 value_2A;
    P01SixByteRecord *value_2C;
} P01F730State;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F6D;
extern P01SixByteRecord *p01_pointer_80220F70;
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_002628E4(s32);
extern void func_00262C64(s16, s16);
extern void func_00262C80(void);
extern void func_0024C204(u8 *, P01Vec3 *, s32);
extern void func_0024BF04(u8, P01Vec3 *, s32);
extern void func_0024C7A4(P01Vec3 *, P01Vec3 *, P01Vec3 *);
extern P01Vec3 *func_0024CDD8(P01SixByteRecord *);
extern float func_001FF7EC(s16, s16);
extern u32 func_00022e60(void);

s32 func_0024F730(u16 message, P01F730State *state)
{
    P01Vec3 initial;
    P01Vec3 *point;
    s16 index;
    s16 noise;
    u16 next;
    s32 alpha;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Keep the first shared-context store before the following reads. */
        *(u8 **)state = p01_context_80220F68;
        state->value_04 = *(u8 *)&p01_pointer_80220F6C;
        state->value_24 = p01_byte_80220F6D;
        state->value_2C = p01_pointer_80220F70;
        if (state->value_24 & 1) {
            state->value_2A = 3;
            state->value_26 = 0x26;
        } else {
            state->value_2A = 1;
            state->value_26 = 0x13;
        }
        state->value_28 = 1;
        func_0024C204(state->context, &state->value_08, 0);
        func_0024BF04(state->value_04, &initial, 0);
        func_0024C7A4(&state->value_14, &initial, &state->value_08);
        state->value_20 = func_0025EF78(&state->value_14);
        func_0025EFC8(&state->value_14);
        func_00262830(0x1000);
        break;
    case 0x1000:
        if (state->value_24 & 1) {
            noise = 0;
            index = 0;
        } else {
            index = state->value_28 % 2;
            noise = (s32)func_00022e60() % 10;
        }
        point = func_0024CDD8(&state->value_2C[index]);
        point->x = state->value_08.x
            + (float)(state->value_26 * state->value_28) * state->value_14.x
            + (float)noise;
        point->z = state->value_08.z
            + (float)(state->value_26 * state->value_28) * state->value_14.z
            + (float)noise;
        point->y = func_001FF7EC((s16)point->x, (s16)point->z);
        state->value_28++;
        next = 0x1000;
        if ((state->value_20 -= (float)state->value_26) <= (float)state->value_26) {
            next = 0x1001;
        }
        if (state->value_24 & 1) {
            alpha = state->value_28 * 255 / 4;
            if (alpha > 255) {
                alpha = 255;
            }
            func_0026CCB4(point->x, point->y, point->z, (s16)alpha);
        }
        func_00262C64(state->value_2A, next);
        break;
    case 0x1001:
        func_00262C80();
        func_002628E4(0x101);
        break;
    }
    return 0;
}
