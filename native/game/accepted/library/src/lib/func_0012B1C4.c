typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef double f64;
typedef struct RuntimeUnit RuntimeUnit;

#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))

extern f64 D_801EE838;
extern f64 D_801EE840;
extern f32 func_8009CFE0(f32 first, f32 second);
extern void func_00106CE0(f32 *first, f32 *second, f32 step);

/* The current direct caller passes four words. The second word is unused
 * by this complete body; its pointee/meaning remains outside this view. */
void func_0012B1C4(RuntimeUnit *owner, void *unused, RuntimeUnit *unit, f32 *point)
{
    f32 previous = FIELD(unit, f32, 0x1C);
    f32 dx, dz;
    f32 step;

    if ((FIELD(owner, u32, 0x00) & 0xC000) != 0 ||
        (FIELD(unit, u32, 0x00) & 0x00010000) != 0) {
        dx = FIELD(unit, f32, 0x08) - point[0];
        dz = FIELD(unit, f32, 0x10) - point[2];
        if (dx == 0.0f) {
            if (dz > 0.0f)
                FIELD(unit, f32, 0x1C) = 0.75f;
            else if (dz < 0.0f)
                FIELD(unit, f32, 0x1C) = 0.25f;
        } else if (dz == 0.0f) {
            if (dx > 0.0f)
                FIELD(unit, f32, 0x1C) = 0.0f;
            else if (dx < 0.0f)
                FIELD(unit, f32, 0x1C) = 0.5f;
        } else {
            f32 value = (f32)(D_801EE840 -
                            (f64)func_8009CFE0(dz, dx) / D_801EE838);
            FIELD(unit, f32, 0x1C) = value;
            if (value < 0.0f) {
                do {
                    FIELD(unit, f32, 0x1C) += 1.0f;
                } while (FIELD(unit, f32, 0x1C) < 0.0f);
            }
            {
                /* Retail carries the positive accumulator; the negative
                 * loop above reloads the stored value on each iteration. */
                f32 rounded = FIELD(unit, f32, 0x1C);
                if (rounded >= 1.0f) {
                    do {
                        rounded -= 1.0f;
                        FIELD(unit, f32, 0x1C) = rounded;
                    } while (rounded >= 1.0f);
                }
            }
        }

        if ((dx == 0.0f) & (dz == 0.0f))
            FIELD(unit, f32, 0x1C) = FIELD(owner, f32, 0x18);

        step = 1.0f;
        if (FIELD(unit, s32, 0x88) != 0) {
            s32 remaining = FIELD(unit, s32, 0x88) - 1;
            FIELD(unit, f32, 0x1C) = previous;
            FIELD(unit, s32, 0x88) = remaining;
            /* Keep the signed guards separate: a combined range expression
             * makes KMC fold these into a different unsigned test. */
            if (remaining >= 0) {
                if (remaining < 6)
                    goto interpolate;
            }
            FIELD(unit, s32, 0x88) = 5;
        } else {
            FIELD(unit, s32, 0x88) = 5;
        }
    } else {
        step = 1.0f;
        FIELD(unit, f32, 0x1C) = FIELD(owner, f32, 0x18);
    }
interpolate:
    func_00106CE0(&FIELD(unit, f32, 0x18), &FIELD(unit, f32, 0x1C), step);
}
