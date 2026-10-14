#include "common/types.h"
typedef float f32;
typedef double f64;
typedef struct Q7Unit Q7Unit;
#define FIELD(p, type, offset) (*(type *)((u8 *)(p) + (offset)))
extern Q7Unit *D_801F0CB0[];
extern f64 D_801EE780, D_801EE788, D_801EE790, D_801EE798, D_801EE7A0;
extern f64 D_801EE7A8, D_801EE7B0, D_801EE7B8, D_801EE7C0, D_801EE7C8;
extern f32 D_801F3A38;
extern s8 D_801F1039[];
extern u16 D_801F1042[];
extern u8 D_801F1052[];
extern void func_001072B8(Q7Unit *);
extern f32 func_8009CFE0(f32, f32);
extern f32 func_00126770(Q7Unit *, Q7Unit *);

static __inline__ f32 wrap_once(f64 wide)
{
    f32 floor = 0.0f;
    f32 value = (f32)wide;
    if (value < floor)
        value += 1.0f;
    if (value >= 1.0f)
        value -= 1.0f;
    return value;
}

void func_00127154(s32 first_index, s32 second_index, s32 mode)
{
    Q7Unit *unit = D_801F0CB0[second_index];
    Q7Unit *other = D_801F0CB0[first_index];
    /* The real result/division/result phases share this working float.
     * Separate scalars change the native register choices. */
    f32 first, second, base, value;
    /* Reuse the actual divisor and fallback-conversion phases. */
    f64 double_work;
    s32 slot;

    if (FIELD(unit, u32, 0) & 2)
        func_001072B8(unit);
    if (FIELD(other, u32, 0) & 2)
        func_001072B8(other);
    FIELD(unit, u32, 0) |= 0x1000;
    second = FIELD(unit, f32, 8) - FIELD(other, f32, 8);
    first = FIELD(other, f32, 0x10) - FIELD(unit, f32, 0x10);
    if (second == 0.0f) {
        if (0.0f < first)
            base = 0.25f;
        else if (first < 0.0f)
            base = 0.75f;
        else
            base = 0.0f;
    } else if (first == 0.0f) {
        if (0.0f < second)
            base = 0.0f;
        else if (second < 0.0f)
            base = 0.5f;
        else
            base = 0.0f;
    } else {
        base = (f32)((f64)func_8009CFE0(first, second) / (double_work = D_801EE780));
    }
    while (base < 0.0f)
        base += 1.0f;
    while (base >= 1.0f)
        base -= 1.0f;
    {
        f32 difference, scaled;
        f64 converted;
        difference = FIELD(unit, f32, 0x18) - base;
        while (difference < 0.0f)
            difference += 1.0f;
        while (difference >= 1.0f)
            difference -= 1.0f;
        converted = (f64)difference;
        /* Keep the single-step normalization in each real range arm.
         * A common inline call shares float constants across the lower loop
         * and changes the emitted instructions. */
        if (D_801EE788 < converted && converted < D_801EE790)
            value = wrap_once(D_801EE798 - converted);
        else {
            double_work = (f64)difference;
            value = wrap_once(D_801EE7A0 - double_work);
        }
        value += base;
        while (value < 0.0f)
            value += 1.0f;
        while (value >= 1.0f)
            value -= 1.0f;
        scaled = D_801F3A38 * 1.5f;
        FIELD(unit, f32, 0xAC) = value;
        FIELD(unit, u8, 0xB4) = first_index;
        FIELD(unit, f32, 0xB0) = scaled;
    }
    if ((FIELD(other, u32, 0) & 0xC0) == 0x40)
        FIELD(unit, f32, 0xAC) = func_00126770(other, unit);
    for (slot = 0; slot < 8; ++slot) {
        if (D_801F1039[slot] == FIELD(unit, u8, 4))
            break;
    }
    if (slot != 8) {
        D_801F1039[slot] = -1;
        D_801F1042[slot] = 0xFFFF;
        D_801F1052[slot] = 0;
    }
    if (mode) {
        f32 difference, scaled;
        FIELD(other, u32, 0) |= 0x1000;
        value = FIELD(unit, f32, 0xB0);
        value /= 2.0f;
        base = (f32)((f64)base - D_801EE7A8);
        FIELD(unit, f32, 0xB0) = value;
        while (base < 0.0f)
            base += 1.0f;
        while (base >= 1.0f)
            base -= 1.0f;
        difference = FIELD(other, f32, 0x18) - base;
        if (difference < 0.0f)
            difference += 1.0f;
        if (D_801EE7B0 < (f64)difference && (f64)difference < D_801EE7B8) {
            value = (f32)(D_801EE7C0 - (f64)difference);
            while (value < 0.0f)
                value += 1.0f;
            while (value >= 1.0f)
                value -= 1.0f;
        } else {
            value = (f32)(D_801EE7C8 - (f64)difference);
        }
        value += base;
        while (value < 0.0f)
            value += 1.0f;
        while (value >= 1.0f)
            value -= 1.0f;
        FIELD(other, f32, 0xAC) = value;
        scaled = FIELD(unit, f32, 0xB0);
        FIELD(other, u8, 0xB4) = second_index;
        FIELD(other, f32, 0xB0) = scaled;
        if ((FIELD(unit, u32, 0) & 0xC0) == 0x40)
            FIELD(other, f32, 0xAC) = func_00126770(unit, other);
        for (slot = 0; slot < 8; ++slot) {
            if (D_801F1039[slot] == FIELD(other, u8, 4))
                break;
        }
        if (slot != 8) {
            D_801F1039[slot] = -1;
            D_801F1042[slot] = 0xFFFF;
            D_801F1052[slot] = 0;
        }
    }
}
