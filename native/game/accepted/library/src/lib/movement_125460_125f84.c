typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef double f64;
typedef struct RuntimeUnit RuntimeUnit;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
extern RuntimeUnit *D_801F0CB0[];
extern u8 D_801F0D98[];
extern f64 D_801EE730, D_801EE738;
extern void resource_free(void *object);
extern f32 func_0002CB80(f32 first, f32 second);
extern f32 func_8009CFE0(f32 first, f32 second);
extern f32 func_801A63FC(s32 selector, f32 first, f32 second);
extern f32 func_801A66C8(s32 selector, f32 first, f32 second);
extern void func_0011AB74(f32 value, RuntimeUnit *unit, f32 *firstOut, f32 *secondOut);
extern f32 func_801B2708(RuntimeUnit *unit, f32 first, f32 second);
extern void func_00106CE0(f32 *first, f32 *second, f32 step);
extern void func_0011B344(RuntimeUnit *unit);
extern void func_801D376C(RuntimeUnit *unit);
extern void func_00106F34(RuntimeUnit *unit);
extern s32 func_801E5730(RuntimeUnit *unit);
extern s32 func_801D92D0(RuntimeUnit *unit);
extern s32 func_801D935C(RuntimeUnit *unit);
extern void func_801DD0E8(RuntimeUnit *unit);
extern void func_801B2B78(RuntimeUnit *unit);
extern s32 func_801DB424(RuntimeUnit *unit, s32 index);

static inline u32 clear_flags(u32 value, u32 mask)
{
    return value & ~mask;
}

static inline void publish_point(RuntimeUnit *unit, f32 x, f32 y, f32 z)
{
    FIELD(unit, f32, 0x0C) = y;
    FIELD(unit, f32, 0x08) = x;
    FIELD(unit, f32, 0x10) = z;
}

/* The current direct caller ignores the result. No float return is inferred
 * from the unrelated value left in f0 on several exits. */
void func_00125460(s32 selector, RuntimeUnit *unit)
{
    u8 *record = FIELD(unit, u8 *, 0xA4);
    /* This real source address is consumed before the later word predicate. */
    u32 comparisonPhase = (u32)((u8 *)unit + 0x08);
    f32 oldX = *(f32 *)comparisonPhase;
    f32 oldZ = FIELD(unit, f32, 0x10);
    f32 oldY = FIELD(unit, f32, 0x0C);
    f32 previous = 0.0f;
    f32 outX, outZ;
    f32 dx, dz;
    s32 completed = 0;

    if (record != 0) {
        f32 height, value;
        previous = FIELD(record, f32, 0x1C);
        func_0011AB74(previous, unit, &outX, &outZ);
        if (FIELD(unit, s32, 0x70) == 1)
            height = func_801A66C8(selector, outX, outZ);
        else
            height = func_801A63FC(selector, outX, outZ);
        publish_point(unit, outX, height, outZ);
        value = func_801B2708(unit, outX, outZ);
        value = FIELD(record, f32, 0x1C) + FIELD(record, f32, 0x18) * (35.0f - value) / 100.0f;
        FIELD(record, f32, 0x1C) = value;
        if (1.0f <= value)
            completed = 1;
    } else {
        f32 distance, value, step;
        if (!(((FIELD(unit, u32, 0x00) & 0x2000) == 0 && FIELD(unit, u8, 0x91) == 1) ||
              ((FIELD(unit, u32, 0x00) & 0x2000) != 0 && FIELD(unit, u8, 0x92) == 1)))
            goto indexed_difference;
        if (FIELD(unit, s32, 0x84) == -1)
            goto indexed_difference;
        dx = FIELD(unit, f32, 0x4C) - FIELD(unit, f32, 0x08);
        dz = FIELD(unit, f32, 0x54) - FIELD(unit, f32, 0x10);
        goto difference_ready;
indexed_difference:
        {
            RuntimeUnit *point = (RuntimeUnit *)((u8 *)unit + (FIELD(unit, s32, 0x24) - 1) * 12);
            dx = FIELD(point, f32, 0x28) - FIELD(unit, f32, 0x08);
            dz = FIELD(point, f32, 0x30) - FIELD(unit, f32, 0x10);
        }
difference_ready:
        distance = func_0002CB80(dx, dz);
        step = 0.01171875f;
        value = func_801B2708(unit, FIELD(unit, f32, 0x08), FIELD(unit, f32, 0x10));
        if (step < distance) {
            f32 factor;
            f32 newZ;
            factor = ((step / distance) * (35.0f - value)) / 100.0f;
            dx *= factor;
            dz *= factor;
            FIELD(unit, f32, 0x08) += dx;
            newZ = FIELD(unit, f32, 0x10) + dz;
            FIELD(unit, f32, 0x10) = newZ;
            if (FIELD(unit, s32, 0x70) == 1)
                FIELD(unit, f32, 0x0C) = func_801A66C8(selector, FIELD(unit, f32, 0x08), newZ);
            else
                FIELD(unit, f32, 0x0C) = func_801A63FC(selector, FIELD(unit, f32, 0x08), newZ);
        } else {
            if (!(((FIELD(unit, u32, 0x00) & 0x2000) == 0 && FIELD(unit, u8, 0x91) == 1) ||
                  ((FIELD(unit, u32, 0x00) & 0x2000) != 0 && FIELD(unit, u8, 0x92) == 1)))
                goto indexed_endpoint;
            if (FIELD(unit, s32, 0x84) == -1)
                goto indexed_endpoint;
            FIELD(unit, f32, 0x08) = FIELD(unit, f32, 0x4C);
            FIELD(unit, f32, 0x10) = FIELD(unit, f32, 0x54);
            goto endpoint_ready;
indexed_endpoint:
            {
                union { s32 index; RuntimeUnit *point; } endpoint;
                /* One actual scratch value has index and address phases;
                 * retain the original accesses without an extra consumer. */
                endpoint.index = FIELD(unit, s32, 0x24);
                endpoint.index -= 1;
                endpoint.point = (RuntimeUnit *)((u8 *)unit + endpoint.index * 12);
                FIELD(unit, f32, 0x08) = FIELD(endpoint.point, f32, 0x28);
                endpoint.index = FIELD(unit, s32, 0x24);
                endpoint.index -= 1;
                endpoint.point = (RuntimeUnit *)((u8 *)unit + endpoint.index * 12);
                FIELD(unit, f32, 0x10) = FIELD(endpoint.point, f32, 0x30);
            }
endpoint_ready:
            if (FIELD(unit, s32, 0x70) == 1)
                FIELD(unit, f32, 0x0C) = func_801A66C8(selector, FIELD(unit, f32, 0x08), FIELD(unit, f32, 0x10));
            else
                FIELD(unit, f32, 0x0C) = func_801A63FC(selector, FIELD(unit, f32, 0x08), FIELD(unit, f32, 0x10));
            completed = 1;
        }
    }

    dx = FIELD(unit, f32, 0x08) - oldX;
    dz = FIELD(unit, f32, 0x10) - oldZ;
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
        f32 value = (f32)(D_801EE738 - (f64)func_8009CFE0(dz, dx) / D_801EE730);
        FIELD(unit, f32, 0x1C) = value;
        if (value < 0.0f) {
            do {
                FIELD(unit, f32, 0x1C) += 1.0f;
            } while (FIELD(unit, f32, 0x1C) < 0.0f);
        }
        {
            f32 value = FIELD(unit, f32, 0x1C);
            if (1.0f <= value) {
                do {
                    value -= 1.0f;
                    FIELD(unit, f32, 0x1C) = value;
                } while (1.0f <= value);
            }
        }
    }
    func_00106CE0(&FIELD(unit, f32, 0x18), &FIELD(unit, f32, 0x1C), 1.0f);
    if (FIELD(unit, f32, 0x18) != FIELD(unit, f32, 0x1C)) {
        FIELD(unit, f32, 0x08) = oldX;
        FIELD(unit, f32, 0x10) = oldZ;
        FIELD(unit, f32, 0x0C) = oldY;
        if (record != 0)
            FIELD(record, f32, 0x1C) = previous;
        completed = 0;
    }

    if (completed != 0) {
        s32 stopped = 0;
        resource_free(FIELD(unit, void *, 0x68));
        FIELD(unit, void *, 0x68) = 0;
        resource_free(FIELD(unit, void *, 0xA0));
        FIELD(unit, void *, 0xA0) = 0;
        func_0011B344(unit);
        if (!(((FIELD(unit, u32, 0x00) & 0x2000) == 0 && FIELD(unit, u8, 0x91) == 1) ||
              ((FIELD(unit, u32, 0x00) & 0x2000) != 0 && FIELD(unit, u8, 0x92) == 1)))
            goto completion_records;
        if (FIELD(unit, s32, 0x84) != -1) {
            FIELD(unit, s32, 0x84) = -1;
            func_801D376C(unit);
            stopped = 1;
            if (FIELD(unit, s32, 0x84) != -1)
                return;
        }
completion_records:
        if ((FIELD(unit, u32, 0x00) & 0x204) == 4) {
            s32 *list = FIELD(unit, s32 *, 0xA8);
            RuntimeUnit *other = D_801F0CB0[FIELD(unit, s32, 0x80)];
            s32 noIdMatch = 1;
            u32 failedFlags;
            if (list != 0 && *list != -1) {
                u8 *limits = D_801F0D98 + 4;
                f32 gridScale = 64.0f;
                /* The pinned target has 32-bit addresses; retain the real
                 * byte offset as the first operand of each list address. */
                s32 offset = 0;
                do {
                    if (*(s32 *)(offset + (u32)list) == FIELD(unit, s32, 0x80)) {
                        if ((FIELD(other, u32, 0x00) & 0x31) == 0x31) {
                            s32 count;
                            s32 addressIndex;
                            f32 lowZ, lowX;
                            f32 z, x;
                            RuntimeUnit *point;
                            RuntimeUnit *slot;
                            addressIndex = FIELD(unit, u8, 0x20);
                            addressIndex -= 1;
                            point = (RuntimeUnit *)((u8 *)unit + addressIndex * 12);
                            FIELD(point, f32, 0x28) = FIELD(other, f32, 0x08);
                            addressIndex = FIELD(unit, u8, 0x20);
                            addressIndex -= 1;
                            point = (RuntimeUnit *)((u8 *)unit + addressIndex * 12);
                            FIELD(point, f32, 0x30) = FIELD(other, f32, 0x10);
                            addressIndex = FIELD(unit, u8, 0x20);
                            addressIndex -= 1;
                            point = (RuntimeUnit *)((u8 *)unit + addressIndex * 12);
                            FIELD(point, f32, 0x2C) = FIELD(other, f32, 0x0C);
                            count = FIELD(unit, u8, 0x20);
                            addressIndex = count;
                            addressIndex -= 1;
                            point = (RuntimeUnit *)((u8 *)unit + addressIndex * 12);
                            lowZ = FIELD(limits, f32, 0);
                            z = (FIELD(point, f32, 0x30) - lowZ) * gridScale / (FIELD(limits, f32, 8) - lowZ);
                            lowX = FIELD(limits, f32, -4);
                            x = (FIELD(point, f32, 0x28) - lowX) * gridScale / (FIELD(limits, f32, 4) - lowX);
                            addressIndex = count;
                            slot = (RuntimeUnit *)((u8 *)unit + addressIndex * 4);
                            FIELD(slot, s32, 0x54) = ((s32)z << 6) + (s32)x;
                            func_00106F34(unit);
                            {
                                s32 current;
                                u32 difference;
                                comparisonPhase = FIELD(unit, u8, 0x20);
                                current = FIELD(unit, volatile s32, 0x24);
                                difference = (u32)current ^ comparisonPhase;
                                comparisonPhase = difference;
                                if ((comparisonPhase != 0) & (stopped == 0))
                                    FIELD(unit, s32, 0x24) = current + 1;
                            }
                            return;
                        }
                        /* Only failed masks reach the flag reader. Keeping
                         * this clear here preserves mask allocation; DBR
                         * speculates it into the retail branch slot. */
                        noIdMatch = 0;
                    }
                    list = FIELD(unit, s32 *, 0xA8);
                    offset += 4;
                } while (*(s32 *)(offset + (u32)list) != -1);
            }
            /* Preserve both real search-failure paths until late cross-jumping;
             * each performs the original flags load once. */
            if (noIdMatch)
                failedFlags = FIELD(unit, u32, 0x00);
            else
                failedFlags = FIELD(unit, u32, 0x00);
            if ((failedFlags & 8) && FIELD(unit, u8, 0x04) < 30)
                FIELD(unit, u32, 0x78) |= 2;
            {
                u32 flags = clear_flags(clear_flags(FIELD(unit, u32, 0x00), 4), 0x200);
                u8 count = FIELD(unit, u8, 0x20);
                FIELD(unit, u32, 0x00) = flags;
                if (FIELD(unit, s32, 0x24) == count) {
                    FIELD(unit, u32, 0x00) = flags & ~2U;
                    FIELD(unit, u8, 0x20) = 0;
                    FIELD(unit, s32, 0x6C) = 0;
                    return;
                }
                FIELD(unit, u8, 0x20) = count - 1;
            }
        }
        if (stopped != 0)
            return;
        if (FIELD(unit, s32, 0x24) != FIELD(unit, u8, 0x20)) {
            FIELD(unit, s32, 0x24) += 1;
            FIELD(unit, s32, 0x6C) = 1;
            return;
        }
        {
            u32 flags = FIELD(unit, u32, 0x00);
            u32 cleared = clear_flags(clear_flags(clear_flags(flags, 4), 0x200), 2);
            FIELD(unit, u8, 0x20) = 0;
            FIELD(unit, s32, 0x6C) = 0;
            FIELD(unit, u32, 0x00) = cleared;
            if (flags & 0x2000) {
                FIELD(unit, u32, 0x00) = cleared & ~0x2000U;
            } else if ((flags & 8) && !(FIELD(unit, u32, 0x78) & 2)) {
                if (FIELD(unit, u8, 0x04) < 30) {
                    s32 value = func_801E5730(unit) & 0xFF;
                    if (value != 0) {
                        FIELD(unit, s32, 0x7C) = value;
                        FIELD(unit, u32, 0x78) |= 0x200000;
                    } else {
                        FIELD(unit, u32, 0x78) |= 1;
                    }
                }
                if ((FIELD(unit, u32, 0x78) & 0x200001) &&
                    (func_801D92D0(unit) == 0 || func_801D935C(unit) != 0))
                    FIELD(unit, u32, 0x78) = clear_flags(clear_flags(FIELD(unit, u32, 0x78), 1), 0x200000) | 0x100000;
            }
        }
        {
        /* These are the actual snapshot mask operands in successive phases;
         * no additional access or use is introduced. */
        u32 snapshotMask = 0x800000;
        if (FIELD(unit, u32, 0x00) & snapshotMask) {
            snapshotMask = ~1U;
            FIELD(unit, u32, 0x78) &= snapshotMask;
        }
        /* Preserve the two actual byte reads and branch directly on the
         * numeric snapshot gate. The views are pinned-compiler workarounds,
         * with no claim of original volatile/MMIO semantics. */
        if (FIELD(unit, volatile u8, 0x04) >= 30 ||
            (u32)(FIELD(unit, u8, 0x92) - 1) >= 2) {
            u32 flags = FIELD(unit, u32, 0x00);
            u32 snapshotBit = 0x800000;
            if (!(flags & snapshotBit)) {
                if (FIELD(unit, volatile u8, 0x04) < 30) {
                    if (FIELD(unit, s32, 0x74) != -1 && FIELD(unit, u8, 0x92) == 0) {
                        f32 x = FIELD(unit, f32, 0x08);
                        f32 y = FIELD(unit, f32, 0x0C);
                        f32 z = FIELD(unit, f32, 0x10);
                        s32 index = FIELD(unit, s32, 0x14);
                        FIELD(unit, u32, 0x00) = flags | snapshotBit;
                        FIELD(unit, f32, 0x4C) = x;
                        FIELD(unit, f32, 0x50) = y;
                        FIELD(unit, f32, 0x54) = z;
                        FIELD(unit, s32, 0x64) = index;
                    } else {
                        FIELD(unit, u32, 0x00) &= ~0x800000U;
                    }
                } else {
                    f32 x = FIELD(unit, f32, 0x08);
                    f32 y = FIELD(unit, f32, 0x0C);
                    f32 z = FIELD(unit, f32, 0x10);
                    s32 index = FIELD(unit, s32, 0x14);
                    FIELD(unit, u32, 0x00) = flags & ~0x800000U;
                    FIELD(unit, f32, 0x4C) = x;
                    FIELD(unit, f32, 0x50) = y;
                    FIELD(unit, f32, 0x54) = z;
                    FIELD(unit, s32, 0x64) = index;
                }
            }
        }
        }
        func_801DD0E8(unit);
        return;
    }

    /* The second existing all-ones test uses an unsigned view to preserve
     * its separate frontend read without a volatile scheduling barrier.
     * No historical field signedness is inferred. */
    if ((FIELD(unit, u32, 0x00) & 0x204) != 4 || FIELD(unit, s32, 0x24) != FIELD(unit, u8, 0x20)) {
        if (!(((FIELD(unit, u32, 0x00) & 0x2000) == 0 && FIELD(unit, u8, 0x91) == 1 && FIELD(unit, s32, 0x84) != -1) ||
              ((FIELD(unit, u32, 0x00) & 0x2000) != 0 && FIELD(unit, u8, 0x92) == 1 && FIELD(unit, u32, 0x84) != 0xFFFFFFFFU)))
            return;
    }
    {
        u32 timer = FIELD(unit, u32, 0x8C);
        s32 next;
        s32 index, position;
        if (timer >= 91) {
            FIELD(unit, u32, 0x8C) = 90;
            return;
        }
        next = timer - 1;
        FIELD(unit, s32, 0x8C) = next;
        if (next >= 0)
            return;
        FIELD(unit, s32, 0x8C) = 90;
        func_801B2B78(unit);
        position = 3;
        if ((FIELD(unit, u32, 0x00) & 0x204) == 4 && FIELD(unit, s32, 0x24) == FIELD(unit, u8, 0x20)) {
            index = FIELD(unit, s32, 0x80);
            position = FIELD(unit, s32, 0x24) - 1;
        } else {
            index = FIELD(unit, s32, 0x84);
        }
        if (func_801DB424(unit, index) != 0) {
            RuntimeUnit *point = (RuntimeUnit *)((u8 *)unit + position * 12);
            FIELD(point, f32, 0x28) = FIELD(D_801F0CB0[index], f32, 0x08);
            FIELD(point, f32, 0x2C) = FIELD(D_801F0CB0[index], f32, 0x0C);
            FIELD(point, f32, 0x30) = FIELD(D_801F0CB0[index], f32, 0x10);
            FIELD(unit, s32, 0x58 + position * 4) = FIELD(D_801F0CB0[index], s32, 0x14);
            return;
        }
        if ((FIELD(unit, u32, 0x00) & 0x204) != 4 || FIELD(unit, s32, 0x24) != FIELD(unit, u8, 0x20))
            FIELD(unit, s32, 0x84) = -1;
    }
}

typedef unsigned short u16;
typedef struct MovementAux MovementAux;
extern RuntimeUnit *D_801F0CB0[];
extern u8 D_800E7A90[];
extern u8 D_801F0D98[];
extern f32 D_801F0D9C, D_801F0DA0, D_801F0DA4;
extern s32 D_801F0DA8, D_801F0DAC;
extern u8 *D_801F0DB0;
extern u16 *D_801F0DB4;
extern u16 D_801F36D8[];
extern u8 D_801EE740[];
extern void *resource_alloc(u32 bytes);
extern void resource_free(void *object);
extern void func_80093380(void *destination, s32 bytes);
extern void func_80093540(const void *message);
extern void func_00106CE0(f32 *first, f32 *second, f32 step);
extern s32 func_00125298(RuntimeUnit *unit);
extern s32 func_00107E38(MovementAux *aux);
extern void func_00125460(s32 selector, RuntimeUnit *unit);
extern void func_001072B8(RuntimeUnit *unit);
extern void func_001289BC(RuntimeUnit *unit);
extern s32 func_0012469C(RuntimeUnit *unit, f32 *point);
extern s32 *func_00107D60(s32 first, s32 second, void *predecessors);
extern void func_0011AECC(RuntimeUnit *unit);

/* The complete owner contains the return delay and a four-byte original
 * alignment tail. This independent research does not supply that tail or
 * change the accepted owner/producer contract. */
void func_00125F84(RuntimeUnit *unit, s32 selector, s32 *state)
{
    MovementAux *aux;
    MovementAux *current;
    s32 *path;
    s32 target;
    f32 point[3];

    if (FIELD(unit, u8, 0x20) == 0) {
        func_00106CE0(&FIELD(unit, f32, 0x18), &FIELD(unit, f32, 0x1C), 1.0f);
        if (FIELD(unit, f32, 0x18) == FIELD(unit, f32, 0x1C)) {
            u32 flags = FIELD(unit, u32, 0x00) & ~2U;
            FIELD(unit, u32, 0x00) = flags;
            if (FIELD(unit, u8, 0x04) < 30) {
                u8 mode = FIELD(unit, u8, 0x92);
                if ((u32)(mode - 1) < 2)
                    return;
                if (FIELD(unit, s32, 0x74) == -1)
                    return;
                if (mode != 0)
                    return;
                {
                    f32 x = FIELD(unit, f32, 0x08);
                    f32 y = FIELD(unit, f32, 0x0C);
                    f32 z = FIELD(unit, f32, 0x10);
                    s32 index = FIELD(unit, s32, 0x14);
                    FIELD(unit, u32, 0x00) = flags | 0x800000;
                    FIELD(unit, f32, 0x4C) = x;
                    FIELD(unit, f32, 0x50) = y;
                    FIELD(unit, f32, 0x54) = z;
                    FIELD(unit, s32, 0x64) = index;
                }
            }
        }
        return;
    }

    current = FIELD(unit, MovementAux *, 0xA0);
    if (current == 0) {
        s32 ready;
        if (*state != 0)
            return;
        aux = FIELD(unit, MovementAux *, 0xA0) = resource_alloc(0x40);
        if (aux != 0) {
            s32 position, value;
            func_80093380(aux, 0x40);
            FIELD(aux, s32, 0x00) = FIELD(unit, s32, 0x14);
            if (!(((FIELD(unit, u32, 0x00) & 0x2000) == 0 && FIELD(unit, u8, 0x91) == 1) ||
                  ((FIELD(unit, u32, 0x00) & 0x2000) != 0 && FIELD(unit, u8, 0x92) == 1)))
                goto indexed_target;
            if (FIELD(unit, s32, 0x84) == -1)
                goto indexed_target;
            position = 3;
            goto target_ready;
indexed_target:
            position = FIELD(unit, s32, 0x24) - 1;
target_ready:
            value = FIELD(unit, s32, 0x58 + position * 4);
            FIELD(aux, s32, 0x10) = 0x3F;
            FIELD(aux, s32, 0x14) = 0x3F;
            FIELD(aux, s32, 0x08) = 0;
            FIELD(aux, s32, 0x0C) = 0;
            FIELD(aux, void *, 0x18) = D_801F0D98;
            FIELD(aux, s32, 0x04) = value;
            FIELD(aux, void *, 0x1C) = resource_alloc(D_801F0DA8 * D_801F0DAC);
            FIELD(aux, void *, 0x20) = resource_alloc(D_801F0DA8 * (D_801F0DAC << 2));
            FIELD(aux, void *, 0x24) = resource_alloc(D_801F0DA8 * (D_801F0DAC << 2));
            {
                s32 height = D_801F0DAC;
                s32 width = D_801F0DA8;
                height *= 12;
                FIELD(aux, void *, 0x30) = resource_alloc(width * height);
            }
            FIELD(aux, void *, 0x3C) = 0;
            ready = 1;
            if (!(FIELD(unit, u32, 0x00) & 0x2000) && FIELD(unit, u8, 0x91) == 2)
                ready = func_00125298(unit);
            FIELD(aux, s32, 0x28) = 0;
            FIELD(aux, s32, 0x2C) = 0;
        } else {
            ready = 0;
        }
        {
            u32 row = FIELD(unit, u32, 0x70) << 6;
            u32 bank = D_800E7A90[0x33];
            D_801F0DB4 = (u16 *)((u32)D_801F36D8 + row);
            D_801F0DB0 = *(u8 **)(D_800E7A90 + bank * 4);
        }
        if (aux == 0 || FIELD(aux, void *, 0x1C) == 0 || FIELD(aux, void *, 0x20) == 0 ||
            FIELD(aux, void *, 0x24) == 0 || ((FIELD(aux, void *, 0x30) == 0) | (ready == 0))) {
            func_80093540(D_801EE740);
            if (aux != 0) {
                if (FIELD(aux, void *, 0x1C) != 0) {
                    resource_free(FIELD(aux, void *, 0x1C));
                    FIELD(aux, void *, 0x1C) = 0;
                }
                if (FIELD(aux, void *, 0x20) != 0) {
                    resource_free(FIELD(aux, void *, 0x20));
                    FIELD(aux, void *, 0x20) = 0;
                }
                if (FIELD(aux, void *, 0x24) != 0) {
                    resource_free(FIELD(aux, void *, 0x24));
                    FIELD(aux, void *, 0x24) = 0;
                }
                if (FIELD(aux, void *, 0x30) != 0) {
                    resource_free(FIELD(aux, void *, 0x30));
                    FIELD(aux, void *, 0x30) = 0;
                }
                if (FIELD(aux, void *, 0x3C) != 0) {
                    resource_free(FIELD(aux, void *, 0x3C));
                    FIELD(aux, void *, 0x3C) = 0;
                }
            }
            if (FIELD(unit, MovementAux *, 0xA0) != 0) {
                resource_free(FIELD(unit, MovementAux *, 0xA0));
                FIELD(unit, MovementAux *, 0xA0) = 0;
            }
            return;
        }
        if (!(FIELD(unit, u32, 0x00) & 0x2000) && FIELD(unit, u8, 0x91) == 2) {
            MovementAux *latest = FIELD(unit, MovementAux *, 0xA0);
            u8 *buffer = FIELD(latest, u8 *, 0x3C);
            if (buffer != 0 && (buffer[FIELD(latest, s32, 0x04)] != 0 || buffer[FIELD(latest, s32, 0x00)] != 0))
                return;
        }
        current = FIELD(unit, MovementAux *, 0xA0);
        goto dispatch;
    }
    if (FIELD(current, s32, 0x28) != 0) {
        if (*state != 0)
            return;
dispatch:
        func_00107E38(current);
        *state = 1;
        return;
    }
    if (!(FIELD(unit, u32, 0x00) & 0x2000) && FIELD(unit, u8, 0x91) == 2)
        FIELD(unit, s32, 0x88) -= 1;
    if (FIELD(unit, s32 *, 0x68) != 0) {
        func_00125460(selector, unit);
        return;
    }
    aux = FIELD(unit, MovementAux *, 0xA0);
    path = 0;
    if (((FIELD(unit, u32, 0x00) & 0x2000) == 0 && FIELD(unit, u8, 0x91) == 1) ||
        ((FIELD(unit, u32, 0x00) & 0x2000) != 0 && FIELD(unit, u8, 0x92) == 1)) {
        if (FIELD(unit, s32, 0x84) != -1) {
            /* These numeric volatile pointer/word views constrain existing reads; they
             * add no access or claim about original qualifiers. */
            u8 *visited = FIELD(aux, u8 * volatile, 0x1C);
            target = FIELD(unit, volatile s32, 0x64);
            if (visited[target] != 0)
                goto construct_path;
            if ((FIELD(unit, u8, 0xB8) & 0xF0) != 0x10) {
                FIELD(unit, u8, 0xB8) = 0x10;
                FIELD(unit, u8, 0xB9) = 2;
                func_001072B8(unit);
                FIELD(unit, s32, 0x84) = -1;
            }
            goto path_ready;
        }
    }
    if (!(FIELD(unit, u32, 0x00) & 0x2000) && FIELD(unit, u8, 0x91) == 2) {
        u8 *buffer = FIELD(aux, u8 *, 0x3C);
        if (buffer != 0) {
            if (buffer[FIELD(aux, s32, 0x00)] != 0) {
                func_001289BC(unit);
                goto path_ready;
            }
            if (buffer[FIELD(aux, s32, 0x04)] != 0)
                goto unavailable_target;
        }
        {
            s32 position = FIELD(unit, s32, 0x24);
            /* These numeric volatile pointer/word views constrain existing reads; they
             * add no access or claim about original qualifiers. */
            u8 *visited = FIELD(aux, u8 * volatile, 0x1C);
            target = *(volatile s32 *)((u32)unit + (position << 2) + 0x54);
            if (visited[target] != 0)
                goto construct_path;
        }
unavailable_target:
        if ((FIELD(unit, u8, 0xB8) & 0xF0) != 0x10) {
            func_001072B8(unit);
            if ((FIELD(unit, u32, 0x00) & 0x08000000) && func_0012469C(unit, point) != -1) {
                f32 lowZ, z, lowX, x;
                f32 pointZ;
                if (FIELD(unit, f32, 0x08) == point[0] && FIELD(unit, f32, 0x10) == point[2])
                    point[2] += 0.0001f;
                FIELD(unit, f32, 0x28) = point[0];
                FIELD(unit, f32, 0x2C) = point[1];
                FIELD(unit, f32, 0x30) = point[2];
                /* Preserve the existing point read before the bounds read,
                 * with input and result in separate source objects. Reusing
                 * z for both phases changes the pinned compiler allocation. */
                pointZ = point[2];
                lowZ = D_801F0D9C;
                z = (pointZ - lowZ) * 64.0f / (D_801F0DA4 - lowZ);
                lowX = FIELD(D_801F0D98, f32, 0);
                x = (point[0] - lowX) * 64.0f / (D_801F0DA0 - lowX);
                FIELD(unit, s32, 0x58) = ((s32)z << 6) + (s32)x;
            } else {
                FIELD(unit, u8, 0xB8) = 0x10;
                FIELD(unit, u8, 0xB9) = 2;
            }
        }
        goto path_ready;
    }
    {
        s32 position = FIELD(unit, s32, 0x24);
        /* These numeric volatile pointer/word views constrain existing reads; they
             * add no access or claim about original qualifiers. */
            u8 *visited = FIELD(aux, u8 * volatile, 0x1C);
        target = *(volatile s32 *)((u32)unit + (position << 2) + 0x54);
        if (visited[target] != 0) {
construct_path:
        path = func_00107D60(FIELD(unit, s32, 0x14), target, FIELD(aux, void *, 0x24));
            goto path_ready;
        }
    }
    if ((FIELD(unit, u8, 0xB8) & 0xF0) != 0x10) {
        FIELD(unit, u8, 0xB8) = 0x10;
        FIELD(unit, u8, 0xB9) = 2;
        func_001072B8(unit);
        if ((FIELD(unit, u32, 0x00) & 0x204) == 4 && FIELD(unit, s32, 0x24) == FIELD(unit, u8, 0x20)) {
            s32 position = FIELD(unit, s32, 0x24) - 1;
            s32 index = FIELD(unit, s32, 0x80);
            RuntimeUnit *view = (RuntimeUnit *)((u8 *)unit + position * 12);
            FIELD(view, f32, 0x28) = FIELD(D_801F0CB0[index], f32, 0x08);
            FIELD(view, f32, 0x2C) = FIELD(D_801F0CB0[index], f32, 0x0C);
            FIELD(view, f32, 0x30) = FIELD(D_801F0CB0[index], f32, 0x10);
            FIELD(unit, s32, 0x58 + position * 4) = FIELD(D_801F0CB0[index], s32, 0x14);
        }
    }
path_ready:
    if (path != 0) {
        FIELD(unit, s32 *, 0x68) = path;
        resource_free(FIELD(aux, void *, 0x1C));
        FIELD(aux, void *, 0x1C) = 0;
        resource_free(FIELD(aux, void *, 0x20));
        FIELD(aux, void *, 0x20) = 0;
        resource_free(FIELD(aux, void *, 0x24));
        FIELD(aux, void *, 0x24) = 0;
        resource_free(FIELD(aux, void *, 0x30));
        FIELD(aux, void *, 0x30) = 0;
        if (FIELD(aux, void *, 0x3C) != 0) {
            resource_free(FIELD(aux, void *, 0x3C));
            FIELD(aux, void *, 0x3C) = 0;
        }
        func_0011AECC(unit);
        FIELD(unit, s32, 0x6C) = 1;
    }
}
