#include "game/effect_adapters_e08.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

typedef struct C57cOffsetPair {
    s8 first;
    s8 second;
} C57cOffsetPair;

typedef s32 (*E08ScalarCallback)(u8 value);
extern E08ScalarCallback g_func_0024BCE8_callbacks[];
extern E08ScalarCallback g_func_0024BF04_callbacks_08[];
extern C57cOffsetPair g_func_0024C57C_offsets_9758[];
extern C57cOffsetPair g_func_0024C57C_offsets_9768[];
extern float func_001FF7EC(s16 first, s16 second);

static inline s32 c57c_category_word(u8 value)
{
    s32 valid;

    if (value < 20) {
        return 0;
    }
    if ((u8)(value - 20) < 27) {
        return 1;
    }

    valid = (u8)(value - 47) < 40;
    if (valid == 0) {
        return (u16)(valid | 255);
    }
    return 2;
}

void func_0024C57C(u8 value, P01Vec3 *output, s32 mode)
{
    s32 callbackIndex;
    s32 loopIndex;
    E08ScalarCallback firstCallback;
    float first;
    float second;
    s16 i;
    /* Preserve the word interface and name its consumed byte view. */
    u8 modeBits = (u8)mode;

    /* The category values 0, 1, 2 and 255 survive both signed views.
     * Keep the two-step forms through early KMC lowering so dispatch and
     * the loop can retain distinct value lifetimes. */
    callbackIndex = (c57c_category_word(value) << 8) >> 8;
    firstCallback = g_func_0024BCE8_callbacks[callbackIndex];
    loopIndex = (callbackIndex << 8) >> 8;

    first = (float)firstCallback(value);
    second = (float)g_func_0024BF04_callbacks_08[callbackIndex](value);
    for (i = 0; i < 4; i++) {
        float firstOffset;
        float secondOffset;

        if (loopIndex == 2) {
            firstOffset = (float)(g_func_0024C57C_offsets_9768[i].first * 19);
            secondOffset = (float)(g_func_0024C57C_offsets_9768[i].second * 19);
        } else {
            firstOffset = (float)(g_func_0024C57C_offsets_9758[i + modeBits * 4].first * 38);
            secondOffset = (float)(g_func_0024C57C_offsets_9758[i + modeBits * 4].second * 38);
        }

        output[i].x = first + firstOffset;
        output[i].z = second + secondOffset;
        output[i].y = func_001FF7EC((s16)(s32)output[i].x,
                                  (s16)(s32)output[i].z);
    }
}
