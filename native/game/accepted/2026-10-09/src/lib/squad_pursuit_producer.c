typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef double f64;
typedef struct RuntimeUnit {
    u32 field_00;
    u8 field_04;
    u8 field_05_to_6F[0x6B];
    u32 field_70;
} RuntimeUnit;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
#define UNIT_FIELD(type, offset) FIELD(unit, type, offset)
#define RECORD_FIELD(record, type, offset) FIELD(record, type, offset)
typedef struct CleanupResource {
    unsigned char field_00_to_1B[0x1C];
    void *field_1C;
    void *field_20;
    void *field_24;
    unsigned char field_28_to_2F[8];
    void *field_30;
    unsigned char field_34_to_3B[8];
    void *field_3C;
} CleanupResource;

typedef struct CleanupObject {
    unsigned int field_00;
    unsigned char field_04_to_67[0x64];
    void *field_68;
    unsigned int field_6C;
    unsigned char field_70_to_87[0x18];
    void *field_88;
    unsigned char field_8C_to_9F[0x14];
    CleanupResource *field_A0;
    unsigned int field_A4;
} CleanupObject;


extern void func_000016C4(void *allocation);
extern void func_001C6C04(void *object);

extern f64 D_801EE090;

void func_00106CE0(f32 *current, f32 *desired, f32 step)
{
    /* Keep the input sample distinct from the working comparison value.
       Its actual final refresh lets pinned KMC keep this sample local and
       preserve the retail floating-point allocation. */
    f32 sampled = *desired;
    f32 lower = (f32)((f64)sampled - D_801EE090);
    f32 upper = (f32)((f64)sampled + D_801EE090);
    f32 center = sampled;
    f32 next;

    if (lower < *current) {
        if (*current < center) {
            f32 value = *current;

            if (center - value < value - lower) {
                next = value + step * 0.033333335f;
                if (center <= next)
                    goto store_and_copy;
                goto store_only;
            } else {
                f32 decreased = value - step * 0.033333335f;

                *current = decreased;
                if (decreased < 0.0f) {
                    next = decreased + 1.0f;
                    goto finish_decreasing;
                }
                return;
            }
        }
    }
    {
        f32 value = *current;

        if (upper - value < value - center) {
            f32 increased = value + step * 0.033333335f;

            *current = increased;
            if (1.0f <= increased) {
                next = increased - 1.0f;
                goto finish_increasing;
            }
            return;
        } else {
            next = value - step * 0.033333335f;
            goto finish_decreasing;
        }
    }

    /* The final desired read follows the store even when the pointers alias. */
finish_increasing:
    if (center <= next)
        goto store_and_copy;
    goto store_only;

finish_decreasing:
    if (next <= center)
        goto store_and_copy;

store_only:
    *current = next;
    return;

store_and_copy:
    /* The desired read must follow the first store when the pointers alias.
       Publishing its chained value through next keeps the final load/store
       in the retail result register with pinned KMC. */
    *current = next;
    *current = next = center = *desired;
}

extern u8 D_800E7AC3;
extern f32 D_801F0D98;
extern f32 D_801F0D9C;
extern f32 D_801F0DA0;
extern f32 D_801F0DA4;
extern u16 D_801F36D8[];
extern f32 D_801E862C[];

f32 func_00106E48(RuntimeUnit *unit, f32 field08, f32 field10)
{
    f32 grid08 = (field08 - D_801F0D98) * 64.0f /
                 (D_801F0DA0 - D_801F0D98);
    f32 grid10 = (field10 - D_801F0D9C) * 64.0f /
                 (D_801F0DA4 - D_801F0D9C);
    u32 selector_address = (u32)&D_800E7AC3;
    u8 *lookup;
    s32 column;
    s32 row;
    u32 cell;
    u32 row_index;
    u32 row_base;
    u16 *row_data;
    u16 value;
    f32 result;

    /* The byte scale is -1020..0. Subtracting it preserves the actual
     * address update while the pinned compiler keeps the base operand first.
     * The emitted update is one positive-scale shift and one addition. */
    selector_address -= (s32)*(u8 *)selector_address * -4;
    lookup = *(u8 **)(selector_address - 0x33);
    column = (s32)grid08;
    row = (s32)grid10;
    cell = ((u32)row << 6) + (u32)column;
    lookup += cell;
    /* Read the row index, then establish its base before forming the stride.
     * This producer order preserves the retail local-register lifetimes. */
    row_index = UNIT_FIELD(u32, 0x70);
    row_base = (u32)D_801F36D8;
    row_data = (u16 *)(row_base + (row_index << 6));
    value = row_data[*lookup];

    if ((value != 0) & (value < 16))
        result = D_801E862C[value];
    else
        result = 2.0f;
    return result + result;
}

extern u8 D_800E7A90[];
extern u16 D_801F36D8[];

void func_00106F34(RuntimeUnit *unit)
{
    s32 recordIndex;
    u8 *map = *(u8 **)(D_800E7A90 + D_800E7A90[0x33] * 4);
    u16 *row = (u16 *)((u32)D_801F36D8 + (FIELD(unit, s32, 0x70) << 6));

    /* Indexed record views keep GCC from rebasing the cursor to field 58.
     * Retain the original signed address test after the unconditional first pass. */
    recordIndex = 0;
    do {
        RuntimeUnit *cursor = (RuntimeUnit *)((u8 *)unit + (recordIndex << 2));
        s32 index = FIELD(cursor, s32, 0x58);
        u8 *cell = map + index;
        if (row[*cell] == 0xFFFF) {
            if (index >= 0x41 && row[cell[-0x41]] != 0xFFFF)
                index -= 0x41;
            else if (index >= 0x40 && row[map[index - 0x40]] != 0xFFFF)
                index -= 0x40;
            else if (index >= 0x3F && row[map[index - 0x3F]] != 0xFFFF)
                index -= 0x3F;
            else if (index > 0 && row[map[index - 1]] != 0xFFFF)
                --index;
            else if (index < 0xFFF && row[map[index + 1]] != 0xFFFF)
                ++index;
            else if (index < 0xFC1 && row[map[index + 0x3F]] != 0xFFFF)
                index += 0x3F;
            else if (index < 0xFC0 && row[map[index + 0x40]] != 0xFFFF)
                index += 0x40;
            else if (index < 0xFBF && row[map[index + 0x41]] != 0xFFFF)
                index += 0x41;
            FIELD(cursor, s32, 0x58) = index;
        }
        ++recordIndex;
    } while ((s32)((u8 *)unit + (recordIndex << 2)) < (s32)((u8 *)unit + 0x0C));
}

extern u8 D_800E7A90[];
extern u16 D_801F36D8[];

void func_001070F4(RuntimeUnit *unit)
{
    s32 recordIndex;
    u8 *map = *(u8 **)(D_800E7A90 + D_800E7A90[0x33] * 4);
    u16 *row = (u16 *)((u32)D_801F36D8 + (FIELD(unit, s32, 0x70) << 6));

    recordIndex = 3;
    do {
        RuntimeUnit *cursor = (RuntimeUnit *)((u8 *)unit + (recordIndex << 2));
        s32 index = FIELD(cursor, s32, 0x58);
        u8 *cell = map + index;
        if (row[*cell] == 0xFFFF) {
            if (index >= 0x41 && row[cell[-0x41]] != 0xFFFF)
                index -= 0x41;
            else if (index >= 0x40 && row[map[index - 0x40]] != 0xFFFF)
                index -= 0x40;
            else if (index >= 0x3F && row[map[index - 0x3F]] != 0xFFFF)
                index -= 0x3F;
            else if (index > 0 && row[map[index - 1]] != 0xFFFF)
                --index;
            else if (index < 0xFFF && row[map[index + 1]] != 0xFFFF)
                ++index;
            else if (index < 0xFC1 && row[map[index + 0x3F]] != 0xFFFF)
                index += 0x3F;
            else if (index < 0xFC0 && row[map[index + 0x40]] != 0xFFFF)
                index += 0x40;
            else if (index < 0xFBF && row[map[index + 0x41]] != 0xFFFF)
                index += 0x41;
            /* A separate indexed store lets GCC reduce every loop-derived
             * address and eliminate the counter in favor of the retail cursor. */
            FIELD(unit, s32, 0x58 + (recordIndex << 2)) = index;
        }
        ++recordIndex;
    } while (recordIndex < 4);
}

extern void func_000016C4(void *allocation);
extern void func_001C6C04(void *object);

void func_001072B8(CleanupObject *object)
{
    if (object->field_A0 != 0) {
        if (object->field_A0->field_1C != 0) {
            func_000016C4(object->field_A0->field_1C);
            object->field_A0->field_1C = 0;
            func_000016C4(object->field_A0->field_20);
            object->field_A0->field_20 = 0;
            func_000016C4(object->field_A0->field_24);
            object->field_A0->field_24 = 0;
            func_000016C4(object->field_A0->field_30);
            object->field_A0->field_30 = 0;
            if (object->field_A0->field_3C != 0) {
                func_000016C4(object->field_A0->field_3C);
                object->field_A0->field_3C = 0;
            }
        }
        func_000016C4(object->field_A0);
        object->field_A0 = 0;
    }
    if (object->field_68 != 0) {
        func_000016C4(object->field_68);
        object->field_68 = 0;
    }
    func_001C6C04(object);
    object->field_88 = 0;
}

extern void func_000016C4(void *allocation);
extern void func_001C6C04(void *object);

void func_0010738C(CleanupObject *object)
{
    if (object->field_A4 == 0) {
        if (object->field_A0 != 0) {
            if (object->field_A0->field_1C != 0) {
                func_000016C4(object->field_A0->field_1C);
                object->field_A0->field_1C = 0;
                func_000016C4(object->field_A0->field_20);
                object->field_A0->field_20 = 0;
                func_000016C4(object->field_A0->field_24);
                object->field_A0->field_24 = 0;
                func_000016C4(object->field_A0->field_30);
                object->field_A0->field_30 = 0;
                if (object->field_A0->field_3C != 0) {
                    func_000016C4(object->field_A0->field_3C);
                    object->field_A0->field_3C = 0;
                }
            }
            func_000016C4(object->field_A0);
            object->field_A0 = 0;
        }
        if (object->field_68 != 0) {
            func_000016C4(object->field_68);
            object->field_68 = 0;
        }
        func_001C6C04(object);
        object->field_88 = 0;
    }
}

extern void func_000016C4(void *allocation);
extern void func_001C6C04(void *object);

void func_0010746C(CleanupObject *object)
{
    object->field_00 &= ~2u;
    object->field_6C = 0;
    *((unsigned char *)object + 0x20) = 0;
    object->field_00 &= ~4u;
    object->field_00 &= ~0x200u;
    if (object->field_A0 != 0) {
        if (object->field_A0->field_1C != 0) {
            func_000016C4(object->field_A0->field_1C);
            object->field_A0->field_1C = 0;
            func_000016C4(object->field_A0->field_20);
            object->field_A0->field_20 = 0;
            func_000016C4(object->field_A0->field_24);
            object->field_A0->field_24 = 0;
            func_000016C4(object->field_A0->field_30);
            object->field_A0->field_30 = 0;
            if (object->field_A0->field_3C != 0) {
                func_000016C4(object->field_A0->field_3C);
                object->field_A0->field_3C = 0;
            }
        }
        func_000016C4(object->field_A0);
        object->field_A0 = 0;
    }
    if (object->field_68 != 0) {
        func_000016C4(object->field_68);
        object->field_68 = 0;
    }
    func_001C6C04(object);
    object->field_88 = 0;
}

extern s32 D_801F367C;
extern u8 D_8018F481;
extern u8 D_801F0DE0[];
extern RuntimeUnit *D_801F0CB0[];

void func_0010756C(void)
{
    s32 index;

    for (index = 0; index < D_801F367C; index++) {
        RuntimeUnit *unit = D_801F0CB0[index];
        u32 flags = unit->field_00;

        if (!(flags & 0x40)) {
            if (flags & 0x3002) {
                flags |= 0x00040000;
                unit->field_00 = flags;
                if (unit->field_04 >= 0x1E) {
                    if (unit->field_70 == 7)
                        unit->field_00 = flags & ~0x00040000;
                }
            } else {
                unit->field_00 = flags & ~0x00040000;
            }
        }
        if (D_8018F481 == 0x36) {
            if (unit->field_04 == 0x1E) {
                flags = unit->field_00;
                if (flags & 0x10)
                    unit->field_00 = flags | 0x00040000;
            }
        }
        if (((s32 *)D_801F0DE0)[0] == 0x1B)
            unit->field_00 &= ~0x00040000;
        if (((s32 *)D_801F0DE0)[0] == 0x1C)
            unit->field_00 &= ~0x00040000;
        }
}

extern RuntimeUnit *D_801F0CB0[];
extern f64 D_801EE098;
extern f32 func_8009CFE0(f32 first, f32 second);

void func_0010766C(RuntimeUnit *unit)
{
    s32 *list = UNIT_FIELD(s32 *, 0xA8);
    f32 minimum = 506.25f;
    s32 best = -1;
    RuntimeUnit *target;

    if (list != 0) {
        s32 index;

        for (index = 0; UNIT_FIELD(s32 *, 0xA8)[index] != -1; index++) {
            s32 id = UNIT_FIELD(s32 *, 0xA8)[index];
            target = D_801F0CB0[id];

            if ((RECORD_FIELD(target, u32, 0x00) & 0x11) == 0x11) {
                f32 dx = UNIT_FIELD(f32, 0x08) - RECORD_FIELD(target, f32, 0x08);
                f32 dz = UNIT_FIELD(f32, 0x10) - RECORD_FIELD(target, f32, 0x10);
                f32 distance = dx * dx + dz * dz;

                if (distance < minimum) {
                    minimum = distance;
                    best = id;
                }
            }
        }
        if (best != -1) {
            target = D_801F0CB0[best];
            {
            f32 first = UNIT_FIELD(f32, 0x10) - RECORD_FIELD(target, f32, 0x10);
            f32 second = RECORD_FIELD(target, f32, 0x08) - UNIT_FIELD(f32, 0x08);
            f32 angle = (f32)((f64)func_8009CFE0(first, second) / D_801EE098);

            while (angle < 0.0f)
                angle += 1.0f;
            while (angle >= 1.0f)
                angle -= 1.0f;
            if ((s32)((angle + 0.0625f) * 8.0f) !=
                (s32)((UNIT_FIELD(f32, 0x18) + 0.0625f) * 8.0f)) {
                UNIT_FIELD(f32, 0x1C) = angle;
                UNIT_FIELD(u32, 0x00) |= 2;
            }
            }
        }
    }
}

