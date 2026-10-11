typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef double f64;
typedef struct RuntimeUnit RuntimeUnit;
typedef struct {
    u8 field00;
    u8 unused01[3];
    f32 field04;
    f32 field08;
} FormationEntry;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))

extern RuntimeUnit *D_801F0CB0[];
extern s32 D_801EB118[][5];
extern FormationEntry D_801EACE0[][5], D_801EAEFC[][5];
extern u8 D_801F0D9C[], D_801F0EBF[], D_801F0ECF[], D_800E7AB9[], D_800E7A90[];
extern f64 D_801EE858, D_801EE860;
extern s32 func_0012E950(s32 value);
extern f32 func_0011AE34(void *record);
extern void func_00126D24(RuntimeUnit *unit);
extern void func_00129948(RuntimeUnit *unit);
extern void func_0012D6FC(RuntimeUnit *unit, u8 selector, s32 *state);
extern void func_0012B1C4(RuntimeUnit *owner, void *unused, RuntimeUnit *unit, f32 *point);
extern s32 func_0012B440(RuntimeUnit *first, RuntimeUnit *second, s32 selector,
                      f32 *current, f32 *requested, s32 bypass);
extern void func_0011A9B8(f32 value, f32 *firstOut, f32 *secondOut, f32 *third,
    f32 *first, f32 *second, f32 *fourth, f32 *fifth, s32 count);
extern f32 func_0002CB80(f32 dx, f32 dz);
extern f32 func_800907F0(f32 angle);
extern f32 func_80092DB0(f32 angle);
extern f32 func_801A63FC(s32 selector, f32 x, f32 z);

static inline f32 member_angle(RuntimeUnit *unit, f64 factor)
{
    f32 value = FIELD(unit, f32, 0x18);
    return (f32)((f64)(value + value) * factor);
}

/* Both passes retain four separate trig calls and the real record reload
 * between the two weights. The later pass's point is still published to
 * its local buffer and its height callback remains observable. */
#define formation_point(unit, entry, factor, scale, point, x, z, weight, heightSource) do { \
    f32 firstProduct, product; \
    (weight) = (entry)->field04 * (scale); \
    firstProduct = func_800907F0(member_angle((unit), (factor))) * (weight); \
    (z) = 0.0f; \
    (x) = firstProduct + (z); \
    product = func_80092DB0(member_angle((unit), (factor))) * (weight); \
    (weight) = (entry)->field08 * (scale); \
    (z) -= product; \
    (x) -= func_80092DB0(member_angle((unit), (factor))) * (weight); \
 \
    (z) -= func_800907F0(member_angle((unit), (factor))) * (weight); \
 \
    (point)[0] = FIELD((unit), f32, 0x08) + (x); \
    (point)[2] = FIELD((unit), f32, 0x10) + (z); \
    (point)[1] = func_801A63FC((heightSource)[0], (point)[0], (point)[2]); \
} while (0)

/* The later pass computes its first angle before the first weight. KMC
 * schedules this separately from the first pass; retain the real reloads. */
#define tail_formation_point(unit, entry, factor, scale, point, x, z, weight, heightSource) do { \
    f32 firstProduct, product, angle; \
    angle = member_angle((unit), (factor)); \
    (weight) = (entry)->field04 * (scale); \
    firstProduct = func_800907F0(angle) * (weight); \
    (z) = 0.0f; \
    (x) = firstProduct + (z); \
    angle = member_angle((unit), (factor)); \
    product = func_80092DB0(angle) * (weight); \
    (weight) = (entry)->field08 * (scale); \
    (z) -= product; \
    angle = member_angle((unit), (factor)); \
    (x) -= func_80092DB0(angle) * (weight); \
 \
    (z) -= func_800907F0(member_angle((unit), (factor))) * (weight); \
 \
    (point)[0] = FIELD((unit), f32, 0x08) + (x); \
    (point)[2] = FIELD((unit), f32, 0x10) + (z); \
    (point)[1] = func_801A63FC((heightSource)[0], (point)[0], (point)[2]); \
} while (0)

static inline s32 point_index(f32 x, f32 z, u8 *bounds)
{
    f32 lowZ = FIELD(bounds, f32, 0);
    f32 gridZ = (z - lowZ) * 64.0f / (FIELD(bounds, f32, 8) - lowZ);
    f32 lowX = FIELD(bounds, f32, -4);
    f32 gridX = (x - lowX) * 64.0f / (FIELD(bounds, f32, 4) - lowX);
    return ((s32)gridZ << 6) + (s32)gridX;
}

/* The retail tail retains bounds/64/map preparations after forming a point.
 * This trial reconstructs a discarded cell test; its historical body and
 * behavioral purpose remain uncertain. Double grid intermediates preserve
 * the observed KMC reload history: this whole query vanishes at late jump
 * deletion, after register allocation, leaving only its preparations. */
static inline s32 tail_point_index(f32 x, f32 z, u8 *bounds, f32 scale)
{
    f32 lowZ = FIELD(bounds, f32, 0);
    f64 gridZ = (z - lowZ) * (f64)scale / (FIELD(bounds, f32, 8) - lowZ);
    f32 lowX = FIELD(bounds, f32, -4);
    f64 gridX = (x - lowX) * (f64)scale / (FIELD(bounds, f32, 4) - lowX);
    return ((s32)gridZ << 6) + (s32)gridX;
}
/* The current caller passes a point, an eleven-byte group record, a state
 * pointer and an initialization word. Numeric fields and observed five-
 * entry strides preserve the existing group/formation interfaces. */
void func_0012BC64(f32 *initialPoint, u8 *group, s32 *state, s32 initialized)
{
    s32 indices[5];
    f32 point[3];
    f32 previous[3];
    u8 members[5];
    RuntimeUnit *leader = 0;
    RuntimeUnit *other;
    /* One definition keeps this real range bound global for KMC reload;
     * a block-local literal instead receives a local FP register. */
    f32 rangeZero = 0.0f;
    f32 progress = 0.0f;
    f32 step = 0.0f;
    /* These real accumulators and weights are reused in both passes;
     * per-pass locals change KMC allocation. Macro arguments are locals. */
    f32 formationX, formationZ, formationWeight;
    s32 special;
    s32 bypass;
    s32 stationary = 0;
    u8 selected = 0;
    s32 initialFlags;
    s32 fillIndex;
    u8 *memberRow;
    s32 pass;
    s32 firstMode;
    u8 *firstBounds;
    u8 *tailBounds;
    RuntimeUnit **tailUnits;
    f32 tailScale;
    f64 factor;
    s32 occupied;
    s32 *firstIndices, *secondIndices;
    union { FormationEntry *formation; RuntimeUnit **units; } entryOrUnits;

    initialFlags = FIELD(D_801F0CB0[func_0012E950(group[2])], s32, 0);
    initialFlags &= 0x4000;
    special = initialFlags != 0;
    if (special == 0)
        selected = group[8];
    memberRow = members;
    switch (selected) {
    case 1: case 2: case 3: case 4:
    case 5: case 6: case 7: case 8:

        members[0] = group[2];
        members[1] = group[3];
        members[2] = group[4];
        members[3] = group[5];
        members[4] = group[6];
        break;
    case 0:
        members[0] = group[2];
        members[2] = group[4];
        if (members[2] == 0xFF) {
            u8 secondMember = group[3];
            members[1] = 0xFF;
            members[2] = secondMember;
        } else {
            members[1] = group[3];
        }
        members[3] = group[5];
        if (members[3] == 0xFF) {
            members[3] = group[6];
            memberRow[4] = 0xFF;
        } else {
            members[4] = group[6];
        }
        break;
    }
    /* The original has no initialization branch for other selector
     * values; no new range guard or default member row is introduced. */
    for (fillIndex = 0; fillIndex < 5; ++fillIndex) {
        if (members[fillIndex] == 0xFF) {
            indices[fillIndex] = -1;
        } else {
            indices[fillIndex] = func_0012E950(members[fillIndex]);
            if (initialized == 0) {
                FIELD(D_801F0CB0[indices[fillIndex]], f32, 0x08) = initialPoint[0];
                FIELD(D_801F0CB0[indices[fillIndex]], f32, 0x0C) = initialPoint[1];
                FIELD(D_801F0CB0[indices[fillIndex]], f32, 0x10) = initialPoint[2];
            }
        }
    }

    pass = 0;
    firstMode = selected;
    factor = D_801EE858;
    firstIndices = indices;
    firstBounds = D_801F0D9C;
    do {
        s32 position = D_801EB118[firstMode][pass];
        RuntimeUnit *unit;
        u32 flags;
        s32 comparison;
        bypass = 0;
        if (members[position] == 0xFF)
            goto next_member;
        entryOrUnits.formation = &D_801EACE0[firstMode][position];
        unit = D_801F0CB0[firstIndices[position]];
        if (position == 0) {
            /* Keep the actual coordinate loads in one multi-set local so
             * KMC does not delay the first load with its single-set boost. */
            f32 coordinate = initialPoint[0];
            void *record;
            leader = unit;
            record = FIELD(leader, void *, 0xA4);
            FIELD(leader, f32, 0x08) = coordinate;
            coordinate = initialPoint[1];
            FIELD(leader, f32, 0x0C) = coordinate;
            coordinate = initialPoint[2];
            FIELD(leader, f32, 0x10) = coordinate;
            if (record != 0)
                step = func_0011AE34(record) * 0.8f;
            goto update_member;
        }
        other = D_801F0CB0[firstIndices[entryOrUnits.formation->field00]];
        if (FIELD(other, u32, 0) & 0x40000)
            FIELD(unit, u32, 0) |= 0x40000;
        else
            FIELD(unit, u32, 0) &= ~0x40000U;
        flags = FIELD(unit, u32, 0);
        if (flags & 0x1000) {
            FIELD(unit, u32, 0) = flags | 0x40000;
            func_00126D24(unit);
            goto next_member;
        }
        previous[0] = FIELD(unit, f32, 0x08);
        previous[1] = FIELD(unit, f32, 0x0C);
        previous[2] = FIELD(unit, f32, 0x10);
        flags = FIELD(unit, u32, 0);
        if (flags & 2)
            goto deferred_movement;
        if (special != 0 && FIELD(leader, void *, 0xA4) != 0) {
            void *record;
            switch (position) {
            case 1: progress = FIELD(FIELD(leader, void *, 0xA4), f32, 0x1C) + (step + step); break;
            case 2: progress = FIELD(FIELD(leader, void *, 0xA4), f32, 0x1C) + step; break;
            case 3: progress = FIELD(FIELD(leader, void *, 0xA4), f32, 0x1C) - step; break;
            case 4: progress = FIELD(FIELD(leader, void *, 0xA4), f32, 0x1C) - (step + step); break;
            }
            if ((rangeZero <= progress) & (progress < 1.0f)) {
                record = FIELD(leader, void *, 0xA4);
                func_0011A9B8(progress, &point[0], &point[2], FIELD(record, f32 *, 8),
                    FIELD(record, f32 *, 0),
                    FIELD(FIELD(leader, void *, 0xA4), f32 *, 4),
                    FIELD(FIELD(leader, void *, 0xA4), f32 *, 12),
                    FIELD(FIELD(leader, void *, 0xA4), f32 *, 16),
                    FIELD(FIELD(leader, void *, 0xA4), s32, 20));
                point[1] = func_801A63FC(D_800E7AB9[0], point[0], point[2]);
                if ((FIELD(leader, u32, 0) & 0xC000) == 0xC000) {
                    if (FIELD(unit, f32, 0x28) != FIELD(unit, f32, 0x08) ||
                        FIELD(unit, f32, 0x2C) != FIELD(unit, f32, 0x0C) ||
                        FIELD(unit, f32, 0x30) != FIELD(unit, f32, 0x10)) {
                        FIELD(unit, u8, 0x20) = 1;
                        FIELD(unit, s32, 0x24) = 1;
                        FIELD(unit, u32, 0) |= 2;
                        FIELD(unit, f32, 0x28) = point[0];
                        FIELD(unit, f32, 0x2C) = point[1];
                        FIELD(unit, f32, 0x30) = point[2];
                        FIELD(unit, s32, 0x58) = point_index(point[0], point[2], firstBounds);
                        goto next_member;
                    }
                } else {
                    bypass = 1;
                }
                goto apply_point;
            }
        }
        formation_point(other, entryOrUnits.formation, factor, 0.8f, point,
                        formationX, formationZ, formationWeight, D_800E7AB9);
apply_point:
        {
            s32 result = func_0012B440(D_801F0CB0[indices[0]], other, group[9],
                                      &FIELD(unit, f32, 0x08), point, bypass);
            if (result > 0) {
                if (result == 1)
                    FIELD(unit, u32, 0) |= 0x40000;
                else {
                    comparison = 2;
                    if (result == comparison)
                        FIELD(unit, u32, 0) &= ~0x10000U;
                }
            } else if (result == -1) {
                FIELD(unit, u32, 0) |= 0x40000;
            } else {
                FIELD(unit, u32, 0) &= ~0x40000U;
            }
        }
        func_0012B1C4(leader, other, unit, previous);
        goto movement_done;
deferred_movement:
        FIELD(unit, u32, 0) = flags | 0x40000;
        func_0012D6FC(unit, group[9], state);
movement_done:
        if ((FIELD(leader, u32, 0) & 0x8000) && !(FIELD(unit, u32, 0) & 2)) {
            if (func_0002CB80(FIELD(unit, f32, 0x08) - previous[0],
                              FIELD(unit, f32, 0x10) - previous[2]) < 0.010986328125f)
                ++stationary;
        }
update_member:
        FIELD(unit, s32, 0x14) = point_index(FIELD(unit, f32, 0x08), FIELD(unit, f32, 0x10), firstBounds);
        if (!(FIELD(unit, u32, 0) & 0x81)) {
            u32 value = FIELD(unit, u8, 0xBA);
            u32 row;
            if (value < 20) {
                flags = value - 4;
                flags = D_801F0EBF[flags * 18];
                comparison = 2;
                if (flags != (u32)comparison)
                    goto member_state_done;
            }
            row = value - 4;
            FIELD(unit, u8, 0xBA) = D_801F0ECF[row * 18];
member_state_done:
            ;
        }
        comparison = FIELD(unit, u32, 0);
        comparison |= 1;
        FIELD(unit, u32, 0) = comparison;
        func_00129948(unit);
        group[1] |= 4;
next_member:
        ++pass;
    } while (pass < 5);

    occupied = 0;
    pass = 0;
    secondIndices = indices;
    tailUnits = D_801F0CB0;
    tailScale = 0.8f;
    factor = D_801EE860;
    progress = 64.0f;
    tailBounds = D_801F0D9C;
    do {
        u8 mode = group[8];
        s32 position = D_801EB118[mode][pass];
        entryOrUnits.formation = &D_801EAEFC[mode][position];
        if (members[position] != 0xFF) {
            ++occupied;
            if (position == 0) {
                leader = tailUnits[indices[0]];
            } else {
                /* The bank byte is at 7AC3, ten bytes after the selector.
                 * This byte view does not establish a settings structure. */
                u8 *heightSelector = D_800E7AB9;
                other = tailUnits[secondIndices[entryOrUnits.formation->field00]];
                tail_formation_point(other, entryOrUnits.formation, factor, tailScale, point,
                                formationX, formationZ, formationWeight, heightSelector);
                {
                    s32 index = tail_point_index(point[0], point[2], tailBounds, progress);
                    u8 *tailMap = D_800E7A90;
                    u32 bank = heightSelector[0x0A];
                    u8 *map = *(u8 **)(tailMap + bank * 4);
                    if (map[index] != 0)
                        formationZ = point[2];
                }
            }
        }
        ++pass;
    } while (pass < 5);
    {
        u32 flags = FIELD(leader, u32, 0);
        if (!(flags & 0x8000)) {
            if (!(flags & 0x1000)) {
                s32 remaining = FIELD(leader, s32, 0x88) - 1;
                FIELD(leader, s32, 0x88) = remaining;
                if (remaining < 0)
                    FIELD(leader, s32, 0x88) = 0;
            }
        } else if (stationary == occupied - 1) {
            FIELD(leader, u32, 0) = flags & ~0x8000U;
        }
    }
}
