#include "game/effect_presentation_p06_internal.h"

typedef struct P06Vec76FE0 { float x, y, z; } P06Vec76FE0;
typedef struct P06Object76FE0 {
    u8 unknown_00[0x1C];
    float value_1C[4][4];
} P06Object76FE0;
typedef struct P06State76FE0 {
    u8 *value_00, *value_04;
    float value_08;
    P06Vec76FE0 value_0C;
    float value_18[4][4];
    P06Vec76FE0 value_58;
    s16 value_64, value_66;
    P06Vec76FE0 value_68;
    P06Object76FE0 *value_74;
} P06State76FE0;

extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C;
extern u8 p06_data_80225730[], p06_data_80225736[], p06_data_8022573C[];
extern float sinf(float);
extern float func_00020bf0(float);
extern void mtx_apply_vec3(float [4][4], float, float, float, float *, float *, float *);
s32 func_00276fe0(u16 message, P06State76FE0 *state)
{
    P06Vec76FE0 first, second, third, fourth, fifth, sixth, seventh;

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
        func_0024C204(state->value_00, (struct P01Vec3 *)(&first), 0);
        func_0024BF04(state->value_04[0], (struct P01Vec3 *)(&second), 0);
        first.y = 0.0f;
        second.y = 0.0f;
        func_0024C7A4(&state->value_68, &second, &first);
        func_0026C230(&state->value_68, state->value_18);
        func_0024C204(state->value_00, (struct P01Vec3 *)(&state->value_0C), 0x20);
        state->value_0C.y += 30.0f;
        func_0025F150(state->value_18, &state->value_0C);
        state->value_08 = 0.0f;
        func_00262830(0x1001);
        break;
    case 0x1001: {
        float radians = state->value_08;
        float x, z;
        /* Keep this case-local angle-to-radian phase in its original carrier. */
        radians *= 0.017453292f;
        x = sinf(radians) * 10.0f;
        z = func_00020bf0(radians) * 10.0f;
        mtx_apply_vec3(state->value_18, 0.0f, x, z, &third.x, &third.y, &third.z);
        func_0024D180(&third, p06_data_80225730);
        func_00262830(0x1002);
        break;
    }
    case 0x1002:
        func_0024BF04(state->value_04[0], (struct P01Vec3 *)(&fourth), 0x10);
        state->value_74 = func_0024D180(&fourth, p06_data_80225736);
        func_00262830(0x1003);
        /* Continue into this message's matrix update. */
    case 0x1003: {
        /* The raw zero word preserves the float ABI without keeping an extra
         * floating zero alive across the two transform calls. */
        union { s32 bits; float value; } zero;
        float angle, radians, x, z, next;
        float (*matrix)[4] = state->value_18;
        float (*objectMatrix)[4] = state->value_74->value_1C;

        zero.bits = 0;
        angle = state->value_08;
        radians = angle * 0.017453292f;
        next = angle + 15.0f;
        x = sinf(radians) * 10.0f;
        z = func_00020bf0(radians) * 10.0f;
        mtx_apply_vec3(matrix, zero.value, x, z,
                       &state->value_58.x, &state->value_58.y, &state->value_58.z);
        if (270.0f <= next) {
            next = 270.0f;
            func_0024BF04(state->value_04[0], (struct P01Vec3 *)(&fifth), 0);
        } else {
            radians = next * 0.017453292f;
            x = sinf(radians) * 10.0f;
            z = func_00020bf0(radians) * 10.0f;
            mtx_apply_vec3(matrix, zero.value, x, z, &fifth.x, &fifth.y, &fifth.z);
        }
        fifth.x -= state->value_58.x;
        fifth.y -= state->value_58.y;
        fifth.z -= state->value_58.z;
        sixth.x = state->value_0C.x - state->value_58.x;
        sixth.y = state->value_0C.y - state->value_58.y;
        sixth.z = state->value_0C.z - state->value_58.z;
        func_0026C1F0(&fifth, &sixth, objectMatrix);
        func_0025F150(objectMatrix, &state->value_58);
        state->value_08 = next;
        if (270.0f <= next) {
            func_00262830(0x1004);
        }
        break;
    }
    case 0x1004:
        func_0024BF04(state->value_04[0], (struct P01Vec3 *)(&seventh), 0);
        func_0024C7A4(&state->value_68, &seventh, &state->value_58);
        /* Narrow directly to the stored halfword; an intermediate int changes
         * the conversion/next-argument schedule and leaves an extra NOP. */
        state->value_64 = (s16)func_0025EF78(&state->value_68);
        state->value_66 = 0;
        func_0025EFC8(&state->value_68);
        func_00262830(0x1005);
        /* Continue with the first movement step. */
    case 0x1005: {
        float newX, newY, newZ;
        s32 step = state->value_66 + 16;
        /* Capture the matrix destination before the clamped movement step. */
        float (*objectMatrix)[4] = state->value_74->value_1C;
        if (state->value_64 - step < 0) {
            step = state->value_64;
        }
        newX = state->value_58.x + state->value_68.x * step;
        newY = state->value_58.y + state->value_68.y * step;
        newZ = state->value_58.z + state->value_68.z * step;
        state->value_58.x = newX;
        state->value_58.y = newY;
        state->value_58.z = newZ;
        func_0025F150(objectMatrix, &state->value_58);
        state->value_66 += 4;
        if (state->value_66 >= 64) {
            state->value_66 = 64;
        }
        state->value_64 -= step;
        if (state->value_64 < 2) {
            func_00262C80();
            func_00262830(0x1006);
        } else {
            func_0024D180(&state->value_58, p06_data_8022573C);
            func_00262830(0x1005);
        }
        break;
    }
    case 0x1006:
        func_002623C4(state->value_74);
        func_00262C64(1, 0x101);
        break;
    }
    return 0;
}
