#include "common/types.h"

typedef struct ResourceSlot {
    u16 unknown00;
    u16 code02;
    s32 key04;
    s32 key08;
    void *data0C;
    u16 next;
    u16 unknown12;
} ResourceSlot;

extern s32 D_80220CA0;
extern ResourceSlot *D_80220CA8;
extern s32 D_80220CAC;
extern s32 D_80220CB0;
extern ResourceSlot *D_80220CB8[];

#define SLOT_MATCHES(slot, kind) \
    ((slot)->data0C != 0 && (slot)->code02 == (kind) && \
     (slot)->key04 == key04 && (slot)->key08 == key08)

#define RETURN_CACHED_SLOT(slot) \
    { \
        if (++D_80220CB0 >= capacity) { \
            D_80220CB0 = 0; \
        } \
        D_80220CB8[D_80220CB0] = (slot); \
        return (slot); \
    }

ResourceSlot *func_00260828(s32 value, s32 key04, s32 key08)
{
    s32 remaining = D_80220CAC;
    s32 index = D_80220CB0;
    s32 ringLength = remaining;
    s32 codeLow;
    ResourceSlot *lower;
    ResourceSlot *middleLower;
    ResourceSlot *middleUpper;
    ResourceSlot *upper;
    s32 capacity;

    if (remaining > 0) {
        do {
            if (index < 0) {
                index = ringLength - 1;
            }
            lower = D_80220CB8[index];
            if (lower != 0 && SLOT_MATCHES(lower, (u16)value)) {
                return lower;
            }
            remaining--;
            index--;
        } while (remaining > 0);
    }

    lower = D_80220CA8;
    remaining = 0;
    index = 0;
    /* Reusing the incoming word for the count after saving its low halfword
     * avoids the extra argument-saving move emitted for a separate count. */
    codeLow = (u16)value;
    value = D_80220CA0;
    capacity = D_80220CAC;
    middleLower = lower + value / 2;
    middleUpper = middleLower + 1;
    upper = lower + (value - 1);
    do {
        if (SLOT_MATCHES(lower, codeLow)) {
            RETURN_CACHED_SLOT(lower);
        }
        if (SLOT_MATCHES(middleLower, codeLow)) {
            RETURN_CACHED_SLOT(middleLower);
        }
        if (SLOT_MATCHES(middleUpper, codeLow)) {
            RETURN_CACHED_SLOT(middleUpper);
        }
        if (SLOT_MATCHES(upper, codeLow)) {
            RETURN_CACHED_SLOT(upper);
        }
        lower++;
        middleLower--;
        middleUpper++;
        upper--;
        if ((u32)middleLower < (u32)lower) {
            index = 1;
        }
        if ((u32)upper < (u32)middleUpper) {
            remaining = 1;
        }
    } while (!(index & remaining));
    return 0;
}
