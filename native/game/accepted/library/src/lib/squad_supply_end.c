typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;

s32 func_00124520(void *unit, s32 selector, s32 member)
{
    s32 result = 0;
    s32 threshold;
    f32 ratio;

    if (member >= 100)
        return 0;
    if (selector == 4)
        return 0;

    switch (selector) {
    case 1:
        ratio = 0.75f;
        threshold = 75;
        break;
    case 2:
        ratio = 0.5f;
        threshold = 75;
        break;
    case 3:
        ratio = 0.25f;
        threshold = 100;
        break;
    default:
        ratio = 0.0f;
        threshold = 100;
        break;
    }

    /* Retain the retail indexed fields under their aligned data base. */
    if ((*(u32 *)unit & 0x08000000) != 0) {
        u8 *row = (u8 *)0x80190000 + member * 52;
        s32 current = *(u16 *)(row + 0x5578);
        if (current != 0) {
            if (current != *(u16 *)(row + 0x5576))
                result = 1;
            else if (row[0x5592] != 0)
                result = 1;
        }
    } else {
        u8 *row = (u8 *)0x80190000 + member * 52;
        /* The halfword fits a signed word, including its float conversion. */
        s32 current = *(u16 *)(row + 0x5578);
        if (current != 0) {
            f32 reference = (f32)*(u16 *)(row + 0x5576);
            if ((f32)current < ratio * reference)
                result = 1;
            if (threshold < row[0x5592])
                result = 1;
        }
    }
    return result;
}


typedef struct RuntimeUnit RuntimeUnit;

#define UNIT(type, offset) (*(type *)((u8 *)unit + (offset)))
extern s32 D_801F1070, D_801F361C;
extern u8 D_800E7AC3, D_8018F481;
extern f32 D_801F0D98[];
extern u16 D_801F36D8[][32];
extern void func_0012EA80(s32 index, f32 *point);
extern s32 func_0013A558(s32 first, s32 second, u8 *a, u8 *b);
extern s32 func_0013AA40(s32 first, s32 second, u8 *a, u8 *b);
extern s32 func_0013B350(s32 first, s32 second, u8 *a, u8 *b);

s32 func_0012469C(RuntimeUnit *unit, f32 *output)
{
    f32 point[3];
    /* Literal default-byte research only: retail has no visible initializer.
     * Do not activate or replace these tests with an invented zero/default skip. */
    u8 helper_output[4];
    s32 count = D_801F1070;
    s32 selected = -1;
    s32 index;
    f32 best = 1012.5f;
        {
            for (index = 0; index < D_801F1070; ++index) {
                f32 x, z, dx, dz, distance;
                s32 column, row, kind;
                u8 *grid;
                u8 *selector = &D_800E7AC3;
                u8 **banks = (u8 **)((u32)selector - 0x33);
                f32 * const bounds = D_801F0D98;
                if ((*(u16 *)((u32)0x80190000 + index * 36 + 0x51CC) & 4) == 0) {
                    if (*(u8 *)((u32)0x80190000 + index * 36 + 0x51C5) != 0)
                        continue;
                }
                func_0012EA80(index, point);
                dx = point[0] - UNIT(f32, 8);
                dz = point[2] - UNIT(f32, 0x10);
                distance = dx * dx + dz * dz;
                x = point[0];
                z = point[2];
                if (!(distance < best))
                    continue;
                grid = banks[*selector];
                if (x < bounds[0])
                    continue;
                if (bounds[2] < x)
                    continue;
                if (z < bounds[1])
                    continue;
                if (bounds[3] < z)
                    continue;
                x -= bounds[0];
                x *= 64.0f;
                x /= bounds[2] - bounds[0];
                z -= bounds[1];
                z *= 64.0f;
                z /= bounds[3] - bounds[1];
                column = (s32)x;
                row = (s32)z;
                kind = UNIT(s32, 0x70);
                if (D_801F36D8[kind][grid[row * 64 + column]] == 0xFFFF)
                    continue;
                if (D_801F361C == 0)
                    goto accept_first;
                if (kind == 1)
                    goto accept_first;
                {
                    switch (D_8018F481) {
                    case 0x27:
                        func_0013A558(UNIT(u8, 0x17), (u8)(row * 64 + column), &helper_output[0], &helper_output[1]);
                        break;
                    case 0x2F:
                    case 0x30:
                    case 0x3C:
                    case 0x3D:
                        func_0013AA40(UNIT(u8, 0x17), (u8)(row * 64 + column), &helper_output[0], &helper_output[1]);
                        break;
                    case 0x33:
                        func_0013B350(UNIT(u8, 0x17), (u8)(row * 64 + column), &helper_output[0], &helper_output[1]);
                        break;
                    }
                    if (helper_output[1] == 0)
                        continue;
                }
            accept_first:
                best = distance;
                selected = index;
            }
        }
    if (selected != -1) {
        func_0012EA80(selected, output);
        return selected;
    }
    {
        count = D_801F1070;
        best = 1012.5f;
        {
            for (index = 0; index < D_801F1070; ++index) {
                f32 x, z, dx, dz, distance;
                s32 column, row, kind;
                u8 *grid;
                u8 *selector = &D_800E7AC3;
                u8 **banks = (u8 **)((u32)selector - 0x33);
                f32 * const bounds = D_801F0D98;
                func_0012EA80(index, point);
                dx = point[0] - UNIT(f32, 8);
                dz = point[2] - UNIT(f32, 0x10);
                distance = dx * dx + dz * dz;
                x = point[0];
                z = point[2];
                if (!(distance < best))
                    continue;
                grid = banks[*selector];
                if (x < bounds[0])
                    continue;
                if (bounds[2] < x)
                    continue;
                if (z < bounds[1])
                    continue;
                if (bounds[3] < z)
                    continue;
                x -= bounds[0];
                x *= 64.0f;
                x /= bounds[2] - bounds[0];
                z -= bounds[1];
                z *= 64.0f;
                z /= bounds[3] - bounds[1];
                column = (s32)x;
                row = (s32)z;
                kind = UNIT(s32, 0x70);
                if (D_801F36D8[kind][grid[row * 64 + column]] == 0xFFFF)
                    continue;
                if (D_801F361C == 0)
                    goto accept_second;
                if (kind == 1)
                    goto accept_second;
                {
                    switch (D_8018F481) {
                    case 0x27:
                        func_0013A558(UNIT(u8, 0x17), (u8)(row * 64 + column), &helper_output[2], &helper_output[3]);
                        break;
                    case 0x2F:
                    case 0x30:
                    case 0x3C:
                    case 0x3D:
                        func_0013AA40(UNIT(u8, 0x17), (u8)(row * 64 + column), &helper_output[2], &helper_output[3]);
                        break;
                    case 0x33:
                        func_0013B350(UNIT(u8, 0x17), (u8)(row * 64 + column), &helper_output[2], &helper_output[3]);
                        break;
                    }
                    if (helper_output[3] == 0)
                        continue;
                }
            accept_second:
                best = distance;
                selected = index;
            }
        }
    }
    if (selected != -1)
        func_0012EA80(selected, output);
    return selected;
}


typedef struct Func001072B8Object Func001072B8Object;

extern u8 D_801971F0[][25];
extern f32 D_801F0D9C, D_801F0DA0, D_801F0DA4;
extern s32 func_0012DA10(RuntimeUnit *unit);
extern s32 func_00123E38(s32 source_word, s32 member);
extern s32 func_00124018(s32 source_word, s32 selector, s32 member);
extern s32 func_00124328(s32 source_word, s32 member);
extern s32 func_00124520(void *unit, s32 selector, s32 member);
extern s32 func_0012469C(RuntimeUnit *unit, f32 *point);
extern void func_001072B8(Func001072B8Object *unit);
extern void func_00121DA8(RuntimeUnit *unit);

s32 func_00124B98(RuntimeUnit *unit)
{
    f32 point[3];
    s32 encountered;
    s32 index;
    s32 selector;
    u8 rejected;
    u8 *record;

    if ((UNIT(u32, 0) & 0x04000000) != 0)
        return 0;

    selector = UNIT(u8, 0xBB);
    record = D_801971F0[UNIT(u8, 4)];
    rejected = func_0012DA10(unit) == 0;
    encountered = 0;
    for (index = 0; index < 5; ++index) {
        s32 member = (record + index)[2];
        s32 below_limit;
        if (member == 0)
            continue;
        below_limit = member < 100;
        if (below_limit) {
            u8 *row = (u8 *)0x80190000 + member * 52;
            if (*(u16 *)(row + 0x5578) == 0)
                continue;
        }
        if (func_00123E38(UNIT(u8, 4), member) != 0)
            return 1;
        if (below_limit) {
            u8 *row = (u8 *)0x80190000 + member * 52;
            if ((row[0x5593] & 4) != 0)
                continue;
        }
        if (func_00124018(UNIT(u8, 4), selector, member) != 0)
            return 2;
        if (func_00124328(UNIT(u8, 4), member) != 0)
            return 3;
        if (rejected != 0)
            continue;
        if ((UNIT(u32, 0) & 0xC0) == 0x40)
            continue;
        if ((UNIT(u32, 0) & 8) != 0)
            continue;
        if (func_00124520(unit, selector, member) == 0)
            continue;
        if ((UNIT(u32, 0) & 0x08000000) != 0) {
            encountered = 1;
            continue;
        }
        if (func_0012469C(unit, point) != -1) {
            f32 x, y, z;
            UNIT(u32, 0) |= 0x08000000;
            UNIT(u8, 0xBC) = UNIT(s32, 0x24);
            UNIT(f32, 0x40) = UNIT(f32, 0x4C);
            UNIT(f32, 0x44) = UNIT(f32, 0x50);
            UNIT(f32, 0x48) = UNIT(f32, 0x54);
            UNIT(s32, 0x60) = UNIT(s32, 0x64);
            /* Keep capture/callback in both real adjustment arms: under the
             * pinned compiler this preserves the observed X/Y/Z reload pattern.
             * The two C sites later merge into one emitted callback. */
            if (UNIT(f32, 8) == point[0] && UNIT(f32, 0x10) == point[2]) {
                f32 adjusted = point[2] + 0.0001f;
                if (adjusted < D_801F0DA4)
                    point[2] = adjusted;
                else
                    point[2] -= 0.0001f;
                x = point[0];
                y = point[1];
                z = point[2];
                func_001072B8((Func001072B8Object *)unit);
            } else {
                x = point[0];
                y = point[1];
                z = point[2];
                func_001072B8((Func001072B8Object *)unit);
            }
            UNIT(u8, 0x91) = 0;
            UNIT(s32, 0x84) = -1;
            UNIT(s32, 0x80) = -1;
            UNIT(s32, 0x88) = 0;
            UNIT(u8, 0x92) = 0;
            /* These views preserve the observed flag-clear/point-store order.
             * They add no accesses and do not establish original qualifiers. */
            UNIT(volatile u32, 0) &= ~2U;
            UNIT(volatile f32, 0x28) = x;
            UNIT(volatile f32, 0x2C) = y;
            UNIT(volatile f32, 0x30) = z;
            z -= D_801F0D9C;
            z *= 64.0f;
            z /= D_801F0DA4 - D_801F0D9C;
            x -= D_801F0D98[0];
            x *= 64.0f;
            x /= D_801F0DA0 - D_801F0D98[0];
            UNIT(s32, 0x24) = (UNIT(u8, 0x20) = 1);
            UNIT(u8, 0x91) = 2;
            UNIT(s32, 0x88) = 90;
            UNIT(u32, 0) &= ~4U;
            UNIT(u32, 0) &= ~0x200U;
            UNIT(u32, 0) |= 2U;
            UNIT(u32, 0) &= ~0x00800000U;
            UNIT(s32, 0x84) = -1;
            UNIT(s32, 0x58) = (s32)z * 64 + (s32)x;
            return 4;
        }
    }
    if (((UNIT(u32, 0) & 0x08000000) != 0) & (encountered == 0)) {
        s32 field64;
        UNIT(u32, 0) &= ~0x08000000U;
        point[0] = UNIT(f32, 0x40);
        point[1] = UNIT(f32, 0x44);
        point[2] = UNIT(f32, 0x48);
        field64 = UNIT(s32, 0x60);
        func_00121DA8(unit);
        UNIT(s32, 0x24) = UNIT(u8, 0xBC);
        UNIT(f32, 0x4C) = point[0];
        UNIT(f32, 0x50) = point[1];
        UNIT(f32, 0x54) = point[2];
        UNIT(s32, 0x64) = field64;
    }
    return 0;
}

