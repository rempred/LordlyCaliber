typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef double f64;
typedef struct RuntimeUnit RuntimeUnit;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))

extern RuntimeUnit *D_801F0CB0[];
extern u8 D_801F0D9C[];
extern f64 D_801EE868, D_801EE870;
extern void func_0011AB74(f32 value, RuntimeUnit *unit, f32 *x, f32 *z);
extern f32 func_801A63FC(s32 selector, f32 x, f32 z);
extern f32 func_00106E48(RuntimeUnit *unit, f32 x, f32 z);
extern f32 func_0002CB80(f32 x, f32 z);
extern f32 func_8009CFE0(f32 first, f32 second);
extern void func_00106CE0(f32 *current, f32 *target, f32 amount);
extern void func_000016C4(void *pointer);
extern void func_0011B344(RuntimeUnit *unit);
extern void func_00106F34(RuntimeUnit *unit);
extern void func_00131828(RuntimeUnit *unit);
extern void func_001072B8(RuntimeUnit *unit);
extern s32 func_0012FB64(RuntimeUnit *unit, s32 index);

static inline f32 float_word(u32 word)
{
    union { u32 word; f32 value; } bits;
    bits.word = word;
    return bits.value;
}

static inline RuntimeUnit *goal_view(RuntimeUnit *unit, s32 index)
{
    return (RuntimeUnit *)((u8 *)unit + index * 12);
}

static inline s32 point_index(f32 x, f32 z, u8 *bounds, f32 scale)
{
    f32 lowZ = FIELD(bounds, f32, 0);
    f32 gridZ = (z - lowZ) * scale / (FIELD(bounds, f32, 8) - lowZ);
    f32 lowX = FIELD(bounds, f32, -4);
    f32 gridX = (x - lowX) * scale / (FIELD(bounds, f32, 4) - lowX);
    return ((s32)gridZ << 6) + (s32)gridX;
}

/* D6FC and D170 supply four words. The first word goes directly to the
 * height callback; its byte-valued current callers do not prove a narrow
 * formal parameter. No residual interpolation/cleanup registers are inputs. */
void func_0012C788(s32 selector, RuntimeUnit *unit, s32 context, s32 mode)
{
    f32 point[2];
    s32 finished = 0;
    void *record = FIELD(unit, void *, 0xA4);
    f32 oldX = FIELD(unit, f32, 8);
    f32 oldZ = FIELD(unit, f32, 16);
    u32 oldY = FIELD(unit, u32, 12);
    u32 oldProgress = 0;
    f32 dx, dz;
    f32 amount;

    if (record != 0) {
        oldProgress = FIELD(record, u32, 0x1C);
        func_0011AB74(float_word(oldProgress), unit, &point[0], &point[1]);
        {
            f32 height = func_801A63FC(selector, point[0], point[1]);
            f32 x = point[0];
            f32 z = point[1];
            FIELD(unit, f32, 12) = height;
            amount = func_00106E48(unit,
                FIELD(unit, f32, 8) = x,
                FIELD(unit, f32, 16) = z);
        }
        if (mode == 0)
            amount *= 10.0f / (11.0f - (f32)context);
        else
            amount = 0.9f;
        if (30.0f < amount)
            amount = 30.0f;
        FIELD(record, f32, 0x1C) += FIELD(record, f32, 0x18) * (35.0f - amount) / 100.0f;
        if (1.0f <= FIELD(record, f32, 0x1C))
            finished = 1;
    } else {
        f32 distance;
        f32 minimumStep;
        union { u32 word; f32 value; } nextZ;
        dx = FIELD(goal_view(unit, FIELD(unit, s32, 0x24) - 1), f32, 0x28) - oldX;
        dz = FIELD(goal_view(unit, FIELD(unit, s32, 0x24) - 1), f32, 0x30) - oldZ;
        distance = func_0002CB80(dx, dz);
        minimumStep = 0.01171875f;
        amount = func_00106E48(unit, FIELD(unit, f32, 8), FIELD(unit, f32, 16));
        if (mode == 0)
            amount *= 10.0f / (11.0f - (f32)context);
        else
            amount = 0.7f;
        if (30.0f < amount)
            amount = 30.0f;
        if (minimumStep < distance) {
            f32 step = (minimumStep / distance) * (35.0f - amount) / 100.0f;
            dx *= step;
            dz *= step;
            FIELD(unit, f32, 8) += dx;
            nextZ.value = FIELD(unit, f32, 16) + dz;
            FIELD(unit, f32, 12) = func_801A63FC(selector, FIELD(unit, f32, 8),
                FIELD(unit, f32, 16) = nextZ.value);
        } else {
            FIELD(unit, f32, 8) = FIELD(goal_view(unit, FIELD(unit, s32, 0x24) - 1), f32, 0x28);
            nextZ.value = FIELD(goal_view(unit, FIELD(unit, s32, 0x24) - 1), f32, 0x30);
            FIELD(unit, f32, 12) = func_801A63FC(selector, FIELD(unit, f32, 8),
                FIELD(unit, f32, 16) = nextZ.value);
            finished = 1;
        }
    }
    dx = FIELD(unit, f32, 8) - oldX;
    dz = FIELD(unit, f32, 16) - oldZ;
    if (dx == 0.0f) {
        if (0.0f < dz)
            FIELD(unit, f32, 0x1C) = 0.75f;
        else if (dz < 0.0f)
            FIELD(unit, f32, 0x1C) = 0.25f;
    } else if (dz == 0.0f) {
        if (0.0f < dx)
            FIELD(unit, f32, 0x1C) = 0.0f;
        else if (dx < 0.0f)
            FIELD(unit, f32, 0x1C) = 0.5f;
    } else {
        FIELD(unit, f32, 0x1C) = (f32)(D_801EE870 - (f64)func_8009CFE0(dz, dx) / D_801EE868);
        while (FIELD(unit, f32, 0x1C) < 0.0f)
            FIELD(unit, f32, 0x1C) += 1.0f;
        while (1.0f <= FIELD(unit, f32, 0x1C))
            FIELD(unit, f32, 0x1C) -= 1.0f;
    }
    if (mode == 0)
        func_00106CE0(&FIELD(unit, f32, 0x18), &FIELD(unit, f32, 0x1C),
            (f32)(6 - context) * 0.1f);
    else
        func_00106CE0(&FIELD(unit, f32, 0x18), &FIELD(unit, f32, 0x1C), 0.5f);
    if (FIELD(unit, f32, 0x18) != FIELD(unit, f32, 0x1C)) {
        FIELD(unit, f32, 8) = oldX;
        FIELD(unit, f32, 16) = oldZ;
        FIELD(unit, u32, 12) = oldY;
        if (record != 0)
            FIELD(record, u32, 0x1C) = oldProgress;
        finished = 0;
    }
    if (finished != 0) {
        func_000016C4(FIELD(unit, void *, 0x68));
        FIELD(unit, void *, 0x68) = 0;
        func_000016C4(FIELD(unit, void *, 0xA0));
        FIELD(unit, void *, 0xA0) = 0;
        func_0011B344(unit);
        if ((FIELD(unit, u32, 0) & 0x204) == 4) {
            s32 *list = FIELD(unit, s32 *, 0xA8);
            RuntimeUnit *target = D_801F0CB0[FIELD(unit, s32, 0x80)];
            s32 noIdMatch = 1;
            u32 failedFlags;
            s32 offset = list != 0;
            if (offset != 0 && *list != -1) {
                u8 *bounds = D_801F0D9C;
                f32 scale = 64.0f;
                offset = 0;
                /* Keep the real byte offset first in address formation. */
                do {
                    if (*(s32 *)((u32)offset + (u32)list) == FIELD(unit, s32, 0x80)) {
                        if ((FIELD(target, u32, 0) & 0x31) == 0x31) {
                            s32 last;
                            RuntimeUnit *goal;
                            u8 *pointBase;
                            FIELD(goal_view(unit, FIELD(unit, u8, 0x20) - 1), f32, 0x28) = FIELD(target, f32, 8);
                            FIELD(goal_view(unit, FIELD(unit, u8, 0x20) - 1), f32, 0x30) = FIELD(target, f32, 16);
                            FIELD(goal_view(unit, FIELD(unit, u8, 0x20) - 1), f32, 0x2C) = FIELD(target, f32, 12);
                            last = FIELD(unit, u8, 0x20);
                            goal = goal_view(unit, last - 1);
                            /* Capture the actual destination before computing its
                             * point index, so the result need not span that work. */
                            pointBase = (u8 *)unit + last * 4;
                            FIELD(pointBase, s32, 0x54) = point_index(FIELD(goal, f32, 0x28), FIELD(goal, f32, 0x30), bounds, scale);
                            func_00106F34(unit);
                            if (FIELD(unit, s32, 0x24) != FIELD(unit, u8, 0x20))
                                ++FIELD(unit, s32, 0x24);
                            return;
                        }
                        /* Successful masks return above; this value is
                         * observed only on search fallthrough. Keep the clear
                         * out of the mask calculation during allocation. */
                        noIdMatch = 0;
                    }
                    offset += 4;
                    list = FIELD(unit, s32 *, 0xA8);
                } while (*(s32 *)((u32)offset + (u32)list) != -1);
            }
            /* Keep the two evidenced failure paths distinct through register
             * allocation. Late cross-jumping merges these equal flags loads,
             * retaining retail no-ID preparations with one executed read. */
            if (noIdMatch)
                failedFlags = FIELD(unit, u32, 0);
            else
                failedFlags = FIELD(unit, u32, 0);
            if (failedFlags & 8)
                FIELD(unit, u32, 0x78) |= 2;
            FIELD(unit, u32, 0) &= ~4U;
            FIELD(unit, u32, 0) &= ~0x200U;
            if (FIELD(unit, s32, 0x24) == FIELD(unit, u8, 0x20)) {
                FIELD(unit, u32, 0) &= ~2U;
                FIELD(unit, u8, 0x20) = 0;
                FIELD(unit, s32, 0x6C) = 0;
                if (!(FIELD(unit, u32, 0) & 8) && !(FIELD(unit, u32, 0x90) & 0x00FFFF00)) {
                    FIELD(unit, u32, 0) |= 0x800000;
                    FIELD(unit, f32, 0x4C) = FIELD(unit, f32, 8);
                    FIELD(unit, f32, 0x50) = FIELD(unit, f32, 12);
                    FIELD(unit, f32, 0x54) = FIELD(unit, f32, 16);
                    FIELD(unit, s32, 0x64) = FIELD(unit, s32, 0x14);
                }
                return;
            }
            --FIELD(unit, u8, 0x20);
        }
        if (FIELD(unit, s32, 0x24) != FIELD(unit, u8, 0x20)) {
            ++FIELD(unit, s32, 0x24);
            FIELD(unit, s32, 0x6C) = 1;
            return;
        }
        {
            u32 flags = FIELD(unit, u32, 0);
            u32 cleared = flags;
            FIELD(unit, u8, 0x20) = 0;
            FIELD(unit, s32, 0x6C) = 0;
            cleared &= ~4U;
            cleared &= ~0x200U;
            cleared &= ~2U;
            cleared &= ~0x40000U;
            FIELD(unit, u32, 0) = cleared;
            /* The masks above preserve bit 8. Its known-clear snapshot
             * enters the actual clear body directly; other paths recheck. */
            if (mode != 0)
                goto recheck_completion_flags;
            if (!(flags & 8))
                goto clear_completion_fields;
            if (FIELD(unit, u32, 0x78) & 2)
                goto recheck_completion_flags;
            {
                u8 value = FIELD(unit, u8, 0xB5);
                if (value != 0xFF) {
                    FIELD(unit, u8, 0xB5) = 0xFF;
                    FIELD(unit, u32, 0) |= 2;
                    FIELD(unit, f32, 0x1C) = (f32)value / 8.0f;
                }
                FIELD(unit, u32, 0x78) |= 1;
                if (FIELD(unit, u32, 0) & 0x800000)
                    FIELD(unit, u32, 0x78) &= ~1U;
            }
            goto completion_flags_done;
recheck_completion_flags:
            if (FIELD(unit, u32, 0) & 8)
                goto completion_flags_done;
clear_completion_fields:
                if (FIELD(unit, u8, 0x91) != 0)
                    FIELD(unit, u8, 0x91) = 0;
                if (!(FIELD(unit, u32, 0x90) & 0x00FFFF00)) {
                    FIELD(unit, u32, 0) |= 0x800000;
                    FIELD(unit, f32, 0x4C) = FIELD(unit, f32, 8);
                    FIELD(unit, f32, 0x50) = FIELD(unit, f32, 12);
                    FIELD(unit, f32, 0x54) = FIELD(unit, f32, 16);
                    FIELD(unit, s32, 0x64) = FIELD(unit, s32, 0x14);
                }
completion_flags_done:
            ;
        }
        func_00131828(unit);
    } else if (((FIELD(unit, u32, 0) & 0x204) == 4 && FIELD(unit, s32, 0x24) == FIELD(unit, u8, 0x20)) ||
               (FIELD(unit, u8, 0x91) == 1 && FIELD(unit, s32, 0x84) != -1)) {
        u32 value = FIELD(unit, u32, 0x8C);
        if (value >= 0x5B) {
            FIELD(unit, s32, 0x8C) = 0x5A;
        } else {
            s32 remaining = value - 1;
            FIELD(unit, s32, 0x8C) = remaining;
            if (remaining < 0) {
                s32 index;
                FIELD(unit, s32, 0x8C) = 0x5A;
                func_001072B8(unit);
                /* The height selector is dead here; this later phase uses
                 * the same local word for its actual goal slot. */
                selector = 3;
                if ((FIELD(unit, u32, 0) & 0x204) == 4 && FIELD(unit, s32, 0x24) == FIELD(unit, u8, 0x20)) {
                    index = FIELD(unit, s32, 0x80);
                    selector = FIELD(unit, s32, 0x24) - 1;
                } else {
                    index = FIELD(unit, s32, 0x84);
                }
                if (func_0012FB64(unit, index)) {
                    RuntimeUnit *goal = goal_view(unit, selector);
                    FIELD(goal, f32, 0x28) = FIELD(D_801F0CB0[index], f32, 8);
                    FIELD(goal, f32, 0x2C) = FIELD(D_801F0CB0[index], f32, 12);
                    FIELD(goal, f32, 0x30) = FIELD(D_801F0CB0[index], f32, 16);
                    FIELD(unit, s32, 0x58 + selector * 4) = FIELD(D_801F0CB0[index], s32, 0x14);
                } else if ((FIELD(unit, u32, 0) & 0x204) != 4 || FIELD(unit, s32, 0x24) != FIELD(unit, u8, 0x20)) {
                    FIELD(unit, s32, 0x84) = -1;
                }
            }
        }
    }
}
