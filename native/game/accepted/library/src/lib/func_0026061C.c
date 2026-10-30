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

/* A do/while (0) wrapper swaps the key and capacity registers in KMC.
 * Keep this return-only macro as a compound block. */
#define RETURN_CACHED_SLOT(slot) \
    { \
        if (++D_80220CB0 >= capacity) { \
            D_80220CB0 = 0; \
        } \
        D_80220CB8[D_80220CB0] = (slot); \
        return (slot); \
    }

ResourceSlot *func_0026061C(void *data)
{
    s32 remaining = D_80220CAC;
    s32 index = D_80220CB0;
    s32 count;
    ResourceSlot *lower;
    ResourceSlot *middleLower;
    ResourceSlot *middleUpper;
    ResourceSlot *upper;
    s32 capacity;

    if (remaining > 0) {
        do {
            if (index < 0) {
                index = D_80220CAC - 1;
            }
            lower = D_80220CB8[index];
            if (lower != 0 && lower->data0C == data) {
                return lower;
            }
            remaining--;
            index--;
        } while (remaining > 0);
    }

    lower = D_80220CA8;
    remaining = 0;
    index = 0;
    count = D_80220CA0;
    capacity = D_80220CAC;
    middleLower = lower + count / 2;
    middleUpper = middleLower + 1;
    upper = lower + (count - 1);
    do {
        if (lower->data0C == data) {
            RETURN_CACHED_SLOT(lower);
        }
        if (middleLower->data0C == data) {
            RETURN_CACHED_SLOT(middleLower);
        }
        if (middleUpper->data0C == data) {
            RETURN_CACHED_SLOT(middleUpper);
        }
        if (upper->data0C == data) {
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
