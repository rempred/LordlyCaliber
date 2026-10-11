typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
extern u8 D_800E7A90[], D_800E7AB9;
extern u16 D_801F36D8[], D_801F3758[];
/* The second dimension records the observed byte-row stride only. */
extern u8 D_801EB520[][32];
extern f32 D_801F0D98, D_801F0D9C, D_801F0DA0, D_801F0DA4;
extern RuntimeUnit *D_801F0CB0[];
extern f32 func_800907F0(f32 value);
extern f32 func_80092DB0(f32 value);
extern f32 func_801A66C8(s32 selector, f32 x, f32 z);
extern f32 func_801A63FC(s32 selector, f32 x, f32 z);
extern void func_001072B8(RuntimeUnit *unit);
extern s32 func_801DBE74(RuntimeUnit *unit);
extern void func_801D376C(RuntimeUnit *unit);
extern void func_00106F34(RuntimeUnit *unit);

/* Returning the shared row preserves its actual lifetime through the
 * compatibility lookup. No row or map capacity is inferred. */
static inline u16 *select_unit_row(RuntimeUnit *unit)
{
    s32 type = FIELD(unit, s32, 0x70);
    if (type == 1)
        return D_801F3758;
    if (type == 7)
        return D_801F3758;
    return (u16 *)((u32)D_801F36D8 + (type << 6));
}

/* Keep the countdown computation within each actual movement path; the
 * compiler shares its flag tail while retaining both float comparisons. */
static inline void update_expiry(RuntimeUnit *unit, f32 step)
{
    f32 remaining = FIELD(unit, f32, 0xB0) - step;
    FIELD(unit, f32, 0xB0) = remaining;
    if (remaining < 0.0f) {
        u32 flags = FIELD(unit, u32, 0x00);
        u32 cleared = flags & ~0x1000U;
        FIELD(unit, u32, 0x00) = cleared;
        if (flags & 0x40)
            FIELD(unit, u32, 0x00) = cleared | 0x10000;
        if (FIELD(unit, u32, 0x00) & 0x80)
            FIELD(unit, u32, 0x00) |= 0x8000;
    }
}

void func_00126D24(RuntimeUnit *unit)
{
    f32 angle = FIELD(unit, f32, 0xAC) * 6.2831855f;
    f32 step = 0.1171875f;
    f32 dx = func_800907F0(angle) * step;
    f32 dz = -(func_80092DB0(angle) * step);
    f32 x = FIELD(unit, f32, 0x08) + dx;
    f32 z = FIELD(unit, f32, 0x10) + dz;
    u16 *row;
    u8 *map;
    f32 lowX, highX, lowZ, highZ;
    f32 inside;
    s32 index;

    row = select_unit_row(unit);
    map = *(u8 **)(D_800E7A90 + D_800E7A90[0x33] * 4);

    /* Keep the observed floating status and ordered comparisons. In
     * particular, an unordered coordinate does not enter the map reads. */
    index = 0;
    lowX = D_801F0D98;
    if (lowX < x) {
        highX = D_801F0DA0;
        if (x < highX) {
            lowZ = D_801F0D9C;
            if (lowZ < z) {
                highZ = D_801F0DA4;
                if (z < highZ) {
                    f32 gridZ = ((FIELD(unit, f32, 0x10) + dz) - lowZ) * 64.0f / (highZ - lowZ);
                    f32 gridX = ((FIELD(unit, f32, 0x08) + dx) - lowX) * 64.0f / (highX - lowX);
                    index = ((s32)gridZ << 6) + (s32)gridX;
                    inside = 1.0f;
                    goto bounds_ready;
                }
            }
        }
    }
    inside = 0.0f;
bounds_ready:
    if (inside != 0.0f) {
        u8 cell = map[index];
        if (row[cell] != 0xFFFF && D_801EB520[map[FIELD(unit, s32, 0x14)]][cell] == 0) {
            f32 height;
            f32 newZ;
            FIELD(unit, f32, 0x08) += dx;
            newZ = FIELD(unit, f32, 0x10) + dz;
            FIELD(unit, f32, 0x10) = newZ;
            if (FIELD(unit, s32, 0x70) == 1)
                height = func_801A66C8(D_800E7AB9, FIELD(unit, f32, 0x08), newZ);
            else
                height = func_801A63FC(D_800E7AB9, FIELD(unit, f32, 0x08), newZ);
            FIELD(unit, f32, 0x0C) = height;
            FIELD(unit, s32, 0x14) = index;
            update_expiry(unit, step);
            goto remaining_ready;
        }
    }
    update_expiry(unit, step);
remaining_ready:
    if (FIELD(unit, u32, 0x00) & 0x1000)
        return;
    func_001072B8(unit);
    if (!(FIELD(unit, u32, 0x00) & 8) && func_801DBE74(unit) != 0)
        return;
    if (FIELD(unit, u8, 0x91) == 1 && FIELD(unit, s32, 0x84) != -1) {
        FIELD(unit, s32, 0x84) = -1;
        func_801D376C(unit);
        if (FIELD(unit, s32, 0x84) != -1)
            return;
    }
    if ((FIELD(unit, u32, 0x00) & 0x206) == 6) {
        if (FIELD(unit, s32, 0x24) == FIELD(unit, u8, 0x20)) {
            s32 position = FIELD(unit, s32, 0x24) - 1;
            RuntimeUnit *other = D_801F0CB0[FIELD(unit, s32, 0x80)];
            RuntimeUnit *point = (RuntimeUnit *)((u8 *)unit + position * 12);
            f32 gridZ, gridX;
            f32 lowerZ, lowerX;
            FIELD(point, f32, 0x28) = FIELD(other, f32, 0x08);
            FIELD(point, f32, 0x30) = FIELD(other, f32, 0x10);
            FIELD(point, f32, 0x2C) = FIELD(other, f32, 0x0C);
            lowerZ = D_801F0D9C;
            gridZ = (FIELD(point, f32, 0x30) - lowerZ) * 64.0f / (D_801F0DA4 - lowerZ);
            lowerX = D_801F0D98;
            gridX = (FIELD(point, f32, 0x28) - lowerX) * 64.0f / (D_801F0DA0 - lowerX);
            FIELD(unit, s32, 0x58 + position * 4) = ((s32)gridZ << 6) + (s32)gridX;
            /* The accepted full helper reads only the unit argument. Its
             * residual a1/a2/a3 values are not additional callee inputs. */
            func_00106F34(unit);
        }
    }
}
