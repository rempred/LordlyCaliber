/* Private complete-NINE candidate preparation under Director2457.
 * Numeric fields and existing aliases only. No group compilation or
 * matching acceptance is established by this authored source. */
#ifndef SCHEDULER_NINE_CONTEXT_H
#define SCHEDULER_NINE_CONTEXT_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef float f32;
typedef double f64;
/* Both observed views cover exactly three floats at unit+8. The array
 * is the existing callback buffer; the scalar view retains numeric fields.
 * This named C89 union preserves both views without a wider callback buffer.
 * Its combined native output still requires the approved producer check. */
typedef union UnitFloats08 {
    struct { f32 field_08, field_0C, field_10; } scalars;
    f32 values[3];
} UnitFloats08;

typedef struct RuntimeUnit {
    u32 flags;
    u8 source_index;
    u8 pad_05[3];
    UnitFloats08 floats_08;
    u32 field_14;
    f32 field_18;
    u8 pad_1C[4];
    u8 field_20;
    u8 pad_21[7];
    f32 points[3][3];
    u8 pad_4C[0x24];
    s32 field_70, field_74;
    u32 field_78;
    s32 field_7C;
    u8 pad_80[0x28];
    s32 *field_A8;
} RuntimeUnit;
typedef RuntimeUnit UnitFlags;
typedef struct MaskView {
    u8 pad_00[8];
    u16 field_08, field_0A, field_0C, field_0E;
} MaskView;
typedef struct FieldView14 {
    u8 pad_00[0x14];
    f32 field_14, field_18, field_1C;
} FieldView14;
typedef struct ListPointState {
    s32 *members;
    f32 point_x, point_y, point_z;
} ListPointState;
typedef struct ResourceRecord {
    u8 pad_00[24];
    s32 field_18;
    u16 field_1C;
    u8 pad_1E[6];
} ResourceRecord;
typedef struct Bounds { f32 field_00, field_04, field_08, field_0C; } Bounds;
typedef struct Record10 { u8 bytes[10]; } Record10;
/* Existing +0 count, ten-byte records, +A2/+A4 words; no capacity claim. */
typedef struct RecordTable {
    u8 count;
    u8 records[16][10];
    u16 primary, secondary;
} RecordTable;
extern RuntimeUnit *D_801F0D28[];
extern s32 D_801F0DE0;
extern s32 D_801F0DE8;
extern RecordTable D_801F0E18;

extern s32 D_801F36C4, D_801F36C8, D_801F36CC;
extern s32 D_801F3658;
extern RuntimeUnit *D_801F0CB0[];
extern u8 D_800E7AC0;
extern void func_000f6b10(void *, f32 *, f32 *, f32 *);
extern f32 hypotf(f32, f32);
extern s32 D_801F3658, D_801F367C;
extern s32 D_801F0BA8, D_801F0BAC;
extern ListPointState D_801F0BB0;
extern MaskView *D_801F0CA0;
extern u16 *D_800E8108;
extern FieldView14 *D_8018F58C;
extern f32 D_801F0DC8, D_801F0DD0;
extern u8 D_801F105E;
extern u8 D_800E7AC0, D_800E7AB9;
extern void *D_800E7A68[];
extern u16 D_801951CC[][18];
extern char D_801EE0F0[];
extern s32 func_00108500(f32 *, f32 *, s32);
extern s32 func_00108AA0(f32, f32, s32);
extern void func_0012EA80(s32, f32 *);
extern f32 func_000F3428(u8, f32, f32);
extern f32 func_000F315C(u8, f32, f32);
extern void *resource_alloc(u32);
extern void resource_free(void *);
extern void func_00023940(char *, ...);
extern s32 D_801F367C;
extern s32 D_801F1070;
extern u8 D_801F0FDE;
extern u8 D_8018F481;
extern f32 D_801F3A38;
extern f32 D_801EB2F0[];
extern s32 func_0005c110(s32);
extern f32 func_00020bf0(f32);
extern f32 sinf(f32);
extern s32 func_801DD244(void);
extern void func_0010ADB8(s32);
extern u8 D_801969BA[][11];
extern s32 func_0012E968(RuntimeUnit *);
extern ResourceRecord D_801951B0[];
extern f64 D_801EE110;
extern Bounds D_801F0D98;
extern u8 D_801F0FE2;
extern u8 D_801F1004[], D_801F1014[], D_801F1024[];
extern u8 D_80196A38[];
extern void func_00118644(u8, s32);
extern s32 func_0012DA9C(RuntimeUnit *);
extern s32 func_0012DA10(RuntimeUnit *);
extern s32 func_0004813c(s32);
extern s32 func_801DD2B0(RuntimeUnit *);
extern u8 *D_801F361C;
extern void *memcpy(void *, void *, u32);
extern void func_00109C3C(RuntimeUnit *);
extern void func_0013A558(u8, u8, u8 *, u8 *);
extern void func_0013AA40(u8, u8, u8 *, u8 *);
extern void func_0013B350(u8, u8, u8 *, u8 *);
extern Record10 D_801F0E19[];
extern u16 D_801F0EBC;
extern u8 D_801E8680;
extern s32 func_0010A128(RuntimeUnit *unit);
extern void func_0010A718(RuntimeUnit *unit);
#endif

/* Original physical order: func_00108AA0. */
s32 func_00108AA0(f32 x, f32 z, s32 mode)
{
    f32 point[3];
    f32 projected_x, projected_z;
    f32 distance;
    s32 selected = -(D_801F36C4 == -1);
    RuntimeUnit **slot;
    u8 *scene_selector;
    u32 entry;
    s32 result;

    if (D_801F36C8 != -1) selected = 1;
    if (D_801F36CC != -1) selected = 2;
    if (selected == -1) return -1;

    slot = &D_801F0CB0[D_801F3658];
    point[0] = (*slot)->points[selected][0];
    point[2] = (*slot)->points[selected][2];
    point[1] = (*slot)->points[selected][1];

    /* The selector byte anchors the pointer table88 bytes earlier.
     * Its negative scale is -1020..0. Subtracting that scale computes the
     * same address while retaining the base-first operand through CSE;
     * COMBINE can then fold it to the positive shift and addition. */
    scene_selector = &D_800E7AC0;
    entry = (u32)scene_selector - (s32)*scene_selector * -4;
    func_000f6b10(*(void **)(entry - 0x58),
                 point, &projected_x, &projected_z);
    distance = hypotf(projected_x - x, projected_z - z);
    /* This genuine success join has incoming edges from both thresholds. */
    if (mode != 0) {
        if (!(distance <= 25.0f)) {
            result = -1;
        } else {
            goto accepted_point;
        }
    } else {
        if (distance <= 12.0f) {
        accepted_point:
            result = 99;
            if (selected != -1) result = selected;
        } else {
            result = -1;
        }
    }
    return result;
}

/* Original physical order: func_00108C1C. */
void func_00108C1C(f32 *x, f32 *z, s32 mode)
{
    f32 point[3];
    f32 adjusted_x, adjusted_z;
    f32 projected_x, projected_z;
    f32 output_x, output_z;
    f32 initial_x, initial_z;
    f32 sum_x, sum_z;
    s32 entry_state = D_801F0DE0;
    s32 state;
    ListPointState *state_view;
    s32 index, selected, count;
    s32 point_count;
    s32 *members;
    s32 *terminator;

    if (((u32)entry_state - 7U) < 2) goto inactive_state;
    if (entry_state != 2) goto active_state;
inactive_state:
    {
        if (entry_state == 2) return;
        {
            s32 *counter = &D_801F0BAC;
            if (*counter != 0) (*counter)++;
        }
        return;
    }

active_state:
    if ((D_801F0CA0->field_0C | D_801F0CA0->field_0E |
         D_801F0CA0->field_08 | D_801F0CA0->field_0A) & D_800E8108[0]) {
        mode = 1;
    }

    initial_x = *x;
    initial_z = *z;
    adjusted_x = initial_x;
    adjusted_z = initial_z;
    selected = func_00108500(&adjusted_x, &adjusted_z, 1);
    if (selected != -1) {
        u16 flags = D_801951CC[selected][0];
        /* Both failed flag gates enter this one publication block. */
        if (!(flags & 8)) goto invalid_initial_flags;
        if (!(flags & 1)) goto invalid_initial_flags;
        D_801F0BA8 = selected;
        goto initial_flags_complete;
    invalid_initial_flags:
        D_801F0BA8 = -1;
    initial_flags_complete:;
    } else {
        D_801F0BA8 = selected;
    }

    if (D_801F0BB0.members != 0) {
        resource_free(D_801F0BB0.members);
        D_801F0BB0.members = 0;
    }
    members = resource_alloc(0xCC);
    /* Retail reports allocation failure, then continues without an added guard. */
    if (members == 0) func_00023940(D_801EE0F0);
    count = 0;

    if (D_801F0DE0 == 1) {

        for (index = 30; index < 50; index++) {
            /* Retail preheaders hoist this loop-body bank invariant after
             * the projection table and before the derived scan pointer. */
            RuntimeUnit **bank = D_801F0CB0;
            /* Derive traversal from the existing bank and index; let the
             * compiler synthesize the actual scan-pointer induction value. */
            u32 work = (u32)bank[index];

            /* Reuse the real selection index across its existing phases.
             * The join copies work into selected, whose initial value is
             * already dead here; no extra producer or consumer is added.
             * Pinned 32-bit ABI: work holds the genuine unit pointer through
             * projection, then the index/sentinel. The target reuses one
             * saved-register lifetime; this scalar representation tests
             * that source hypothesis without a union field-store temp.
             * Both unused distance calls remain. */
            hypotf(D_8018F58C->field_14 - D_801F0DC8,
                   D_8018F58C->field_1C - D_801F0DD0);
            hypotf(D_8018F58C->field_14 - ((RuntimeUnit *)work)->floats_08.values[0],
                   D_8018F58C->field_1C - ((RuntimeUnit *)work)->floats_08.values[2]);
            func_000f6b10(D_800E7A68[D_800E7AC0], ((RuntimeUnit *)work)->floats_08.values,
                         &projected_x, &projected_z);
            work = (u32)(hypotf(projected_x - adjusted_x,
                               projected_z - adjusted_z) <= 25.0f ? index : -1);
            selected = (s32)work;
            if (selected != -1) {
                if ((bank[selected]->flags & 0x0839) == 0x31) {
                    /* The allocation and count already define this append.
                     * Test native loop strength reduction instead of carrying
                     * an explicit second pointer through the scan. */
                    members[count++] = selected;
                }
            }
        }
        /* Complete this branch's real sentinel list. The common late tail
         * may be merged by the compiler after reference/lifetime accounting. */
        /* Pinned 32-bit address arithmetic keeps the observed scaled-count
         * first addition; ordinary C pointer addition normalizes base first.
         * One real terminator variable covers both completed scan branches. */
        terminator = (s32 *)(((u32)count * 4U) + (u32)members);
        selected = -1;
        *terminator = selected;
        /* Define the genuine publication/point base on each completed scan
         * path. The late join can share the address producer. */
        state = D_801F0DE0;
        state_view = &D_801F0BB0;
    } else {

        for (index = 0; index < D_801F367C; index++) {
            /* Retail preheaders hoist this loop-body bank invariant after
             * the projection table and before the derived scan pointer. */
            RuntimeUnit **bank = D_801F0CB0;
            /* Derive traversal from the existing bank and index; let the
             * compiler synthesize the actual scan-pointer induction value. */
            u32 work = (u32)bank[index];

            hypotf(D_8018F58C->field_14 - D_801F0DC8,
                   D_8018F58C->field_1C - D_801F0DD0);
            hypotf(D_8018F58C->field_14 - ((RuntimeUnit *)work)->floats_08.values[0],
                   D_8018F58C->field_1C - ((RuntimeUnit *)work)->floats_08.values[2]);
            func_000f6b10(D_800E7A68[D_800E7AC0], ((RuntimeUnit *)work)->floats_08.values,
                         &projected_x, &projected_z);
            work = (u32)(hypotf(projected_x - adjusted_x,
                               projected_z - adjusted_z) <= 25.0f ? index : -1);
            selected = (s32)work;
            if (selected != -1) {
                if ((bank[selected]->flags & 0x40001031) == 0x31) {
                    /* The allocation and count already define this append.
                     * Test native loop strength reduction instead of carrying
                     * an explicit second pointer through the scan. */
                    members[count++] = selected;
                }
            }
        }
        /* Pinned 32-bit address arithmetic keeps the observed scaled-count
         * first addition; ordinary C pointer addition normalizes base first.
         * One real terminator variable covers both completed scan branches. */
        terminator = (s32 *)(((u32)count * 4U) + (u32)members);
        selected = -1;
        *terminator = selected;
        /* Define the genuine publication/point base on each completed scan
         * path. The late join can share the address producer. */
        state = D_801F0DE0;
        state_view = &D_801F0BB0;
    }

    state_view->members = members;
    if ((u32)state >= 2) {
        if (D_801F0BAC != 0) {
            D_801F0BAC++;
            return;
        }
        if (state == 4) {
            /* Static address evidence: the existing BAC word is four bytes
             * below this BB0 view. Pinned 32-bit address arithmetic expresses
             * retail's pointer-relative store without merging data owners. */
            *(s32 *)((u32)state_view - 4U) = 1;
        }
        return;
    }

    sum_z = 0.0f;
    sum_x = sum_z;
    /* List membership and point accumulation count different real things.
     * Their lifetimes are disjoint; keep the later counter distinct. */
    point_count = 0;
    state_view->point_z = 0.0f;
    state_view->point_y = 0.0f;
    state_view->point_x = 0.0f;
    /* The scan counter is dead before these point/member-index phases.
     * Reuse that genuine index value, as suggested by retail S1 roles. */
    if (D_801F3658 != selected) {
        index = func_00108AA0(initial_x, initial_z, 1);
        if ((index != -1) & (index != 99)) {
            /* This point slot dies before projection; it is not the scan cursor. */
            RuntimeUnit **point_slot;
            f32 point_x, point_y, point_z;
            f32 next_y;
            /* The existing table is 0x58 bytes before the selector byte. */
            u8 *selector;
            void **table;
            u32 projection_index;
            point_slot = &D_801F0CB0[D_801F3658];
            /* Retail consumes the captured point values in the global adds
             * and delays the Y array write until those adds are complete. */
            point_x = (*point_slot)->points[index][0];
            point[0] = point_x;
            point_z = (*point_slot)->points[index][2];
            point[2] = point_z;
            point_y = (*point_slot)->points[index][1];
            next_y = state_view->point_y + point_y;
            selector = &D_800E7AC0;
            table = (void **)(selector - 0x58);
            /* Retail reads the actual selector byte before the float writes,
             * while the selected table pointer is consumed after them. */
            projection_index = *selector;
            state_view->point_x += point_x;
            state_view->point_z += point_z;
            point[1] = point_y;
            state_view->point_y = next_y;
            func_000f6b10(table[projection_index], point, &output_x, &output_z);
            sum_x += output_x - initial_x;
            sum_z += output_z - initial_z;
            point_count = 1;
        }
    }

    /* Preserve the global head test, then freshly reload the member cursor.
     * The earlier allocated pointer is dead after publication; retail uses
     * the same saved-register role for this later sentinel walk. */
    if (D_801F0BB0.members[0] != -1) {
        members = D_801F0BB0.members;

        index = *members++;
        if (index != -1) {
            do {
                RuntimeUnit **bank;
                u8 *selector;
                void **table;
                /* The member slot dies before height/projection calls. */
                RuntimeUnit **member_slot;
                f32 height;
                f32 value_z, base_x, value_x, base_z, next_z;

                if (D_801F3658 != -1) {
                    if (D_801F105E != 1) goto next_member;
                }
                bank = D_801F0CB0;
                member_slot = &bank[index];
                point[0] = (*member_slot)->floats_08.values[0];
                point[2] = (*member_slot)->floats_08.values[2];
                if ((*member_slot)->field_70 == 1) {
                    height = func_000F3428(D_800E7AB9, point[0], point[2]);
                } else {
                    height = func_000F315C(D_800E7AB9, point[0], point[2]);
                }
                /* Capture the actual two sum inputs before either write. */
                base_x = D_801F0BB0.point_x;
                value_x = point[0];
                base_z = D_801F0BB0.point_z;
                value_z = point[2];
                D_801F0BB0.point_x = base_x + value_x;
                selector = &D_800E7AC0;
                table = (void **)(selector - 0x58);
                next_z = base_z + value_z;
                D_801F0BB0.point_y += height;
                /* Retail retains the real height in a float register until
                 * this delayed point-array write, after the three adds. */
                point[1] = height;
                /* Delay the real Z publication until the other values are ready.
                 * This preserves retail's load order and float allocation. */
                D_801F0BB0.point_z = next_z;
                func_000f6b10(table[*selector], point, &output_x, &output_z);
                sum_x += output_x - initial_x;
                sum_z += output_z - initial_z;
                point_count++;
            next_member:
                index = *members++;
            } while (index != -1);
        }
    }

    selected = D_801F0BA8;
    if (selected != -1) {
        u8 *selector;
        void **table;
        if (D_801F3658 != -1) {
            if (D_801F105E != 0) goto finish;
        }
        func_0012EA80(selected, point);
        /* This immutable address is needed only after the opaque producer;
         * retail constructs it in caller registers in this phase. */
        selector = &D_800E7AC0;
        table = (void **)(selector - 0x58);
        {
            /* Same-block base lets the compiler fold each genuine field address. */
            ListPointState *selected_state = &D_801F0BB0;
            selected_state->point_x += point[0];
            selected_state->point_y += point[1];
            selected_state->point_z += point[2];
        }
        func_000f6b10(table[*selector], point, &output_x, &output_z);
        sum_x += output_x - initial_x;
        sum_z += output_z - initial_z;
        point_count++;
    }

finish:
    if (point_count != 0) {
        /* Retail normalizes both accumulated deltas on every nonempty path;
         * only applying the resulting displacement depends on mode. */
        sum_x /= (f32)point_count;
        D_801F0BB0.point_x /= (f32)point_count;
        D_801F0BB0.point_y /= (f32)point_count;
        D_801F0BB0.point_z /= (f32)point_count;
        sum_z /= (f32)point_count;
        if (mode == 0) {
            *x += sum_x / 4.0f;
            *z += sum_z / 4.0f;
        }
        D_801F0BAC++;
    } else {
        s32 *counter = &D_801F0BAC;
        if (*counter != 0) (*counter)--;
    }
    /* This is an unsigned range test in the original, including negative states. */
    if ((u32)D_801F0DE8 >= 2) {
        if (D_801F0DE8 != 2) D_801F0BAC = 0;
    }
}

/* Original physical order: func_001094BC. */
void func_001094BC(void)
{
    s32 initial_mode = D_801F0DE0;
    s32 index;
    /* The two fixed scans use these real values in nonoverlapping phases.
     * Keeping common identities preserves the observed counter, entry and
     * bound register roles with the pinned compiler. */
    s32 scan_index;
    RuntimeUnit *entry;
    s32 available;
    f32 point[3];
    f32 radius;
    f32 min_x, max_x, min_z, max_z;

    if (initial_mode == 2) return;
    if (initial_mode == 17) return;

    for (index = 0, available = 0; index < D_801F367C; index++) {
            RuntimeUnit *unit = D_801F0CB0[index];
            u32 initial_flags = unit->flags;
            if (!(initial_flags & 8)) {
                if (D_8018F481 != 0x3F || !(initial_flags & 0x01000000)) {
                    if (func_0005c110(28)) unit->flags |= 0x20;
                    else unit->flags &= ~0x20;
                }
                {
                    u32 counted_flags = unit->flags;
                    if (!(counted_flags & 8) && unit->source_index >= 30)
                        available += counted_flags & 1;
                }
            }
            if (D_801F3658 == -1) {
                unit->flags &= ~0x00100000;
            } else if (index == D_801F3658) {
                unit->flags &= ~0x00100000;
            } else {
                /* The scalar flag-word view retains its ordering with the
                 * subsequent opaque state-bank read in this compiler. */
                u32 *flag_word = (u32 *)unit;
                u32 marked_flags = *flag_word | 0x00100000;
                *flag_word = marked_flags;
                if (D_801F0DE0 == 1 && !(marked_flags & 8))
                    *flag_word = marked_flags & ~0x00100000;
            }
    }
    D_801F0FDE = available;

    for (index = 0; index < D_801F367C; index++) {
            /* Preserve the actual base used by the outer fetch, member lookup
             * and fixed scan. An implicit base rematerializes the +120 cursor
             * as an absolute address instead of the retail saved-base ADDIU. */
            u32 second_bank_address = (u32)D_801F0CB0;
            RuntimeUnit *unit = *(RuntimeUnit **)(second_bank_address + ((u32)index << 2));
            u32 flags = unit->flags;
            if (flags & 8) {
            if (flags & 1) {
            if (flags & 0x10) {
                if (func_801DD244()) {
                    s32 *members = unit->field_A8;
                    if (members != 0 && members[0] != -1) {
                        s32 terminator = -1;
                        s32 offset = 0;
                        do {
                            RuntimeUnit *member = *(RuntimeUnit **)(((u32)members[offset] << 2) + second_bank_address);
                            member->flags |= 0x20;
                            members = unit->field_A8;
                            offset++;
                        } while (members[offset] != terminator);
                    }
                } else {
                    f32 center_x;
                    f32 center_z;
                    RuntimeUnit **other_cursor;
                    if (!(unit->flags & 0x00020000)) {
                        f32 shift_x;
                        f32 shift_z;
                        shift_x = D_801F3A38 * 1.5f;
                        shift_x *= func_00020bf0(unit->field_18 * 6.2831855f);
                        shift_z = D_801F3A38 * 1.5f;
                        center_x = unit->floats_08.scalars.field_08 + shift_x;
                        shift_z *= sinf(unit->field_18 * 6.2831855f);
                        radius = D_801F3A38 * D_801EB2F0[unit->field_70];
                        center_z = unit->floats_08.scalars.field_10 - shift_z;
                    } else {
                        center_x = unit->floats_08.scalars.field_08;
                        center_z = unit->floats_08.scalars.field_10;
                        radius = D_801F3A38 * 1.2f;
                    }
                    min_x = center_x - radius;
                    max_x = center_x + radius;
                    min_z = center_z - radius;
                    max_z = center_z + radius;
                    scan_index = 30;
                    other_cursor = (RuntimeUnit **)(second_bank_address + 120U);
                    /* Keep the real counted mark path separate from the
                     * failure increment and final flag publication. The two
                     * delay-slot relaxation passes otherwise merge the count
                     * one guard too early or fill the distance slot with ORI. */
                    do {
                        u32 other_flags;
                        f32 x, z;
                        entry = *other_cursor;
                        other_flags = entry->flags;
                        if (other_flags & 8) goto first_advance_count;
                        if ((other_flags & 0x31) != 0x11) goto first_advance_count;
                        x = entry->floats_08.scalars.field_08;
                        if (!(min_x < x)) goto first_advance_count;
                        if (!(x < max_x)) goto first_advance_count;
                        z = entry->floats_08.scalars.field_10;
                        if (!(min_z < z)) goto first_advance_count;
                        if (!(z < max_z)) goto first_advance_count;
                        {
                            f32 dx = x - center_x;
                            f32 dz = z - center_z;
                            if (!(dx * dx + dz * dz < radius * radius))
                                goto first_advance_count;
                            scan_index++;
                            goto first_mark_flags;
                        }
first_advance_count:
                        scan_index++;
                        goto first_advance_cursor;
first_mark_flags:
                        entry->flags = other_flags | 0x20;
first_advance_cursor:
                        other_cursor++;
                    } while (scan_index < 50);
                }
            }
            }
            }
    }

    radius = D_801F3A38;
    for (index = 0; index < D_801F1070; index++) {
            u16 *record = D_801951CC[index];
            /* This real producer retains the callback-crossing bank base.
             * The flag mask below has its own later, local lifetime. */
            u32 bank_address = (u32)D_801F0CB0;
            if (*record & 2) {
                RuntimeUnit **other_cursor;
                /* Capture the unsigned byte in its comparison width. A QI
                 * local defers zero extension and changes preheader order. */
                u32 stage;
                u32 keep_flags;
                func_0012EA80(index, point);
                min_x = point[0] - radius;
                max_x = point[0] + radius;
                scan_index = 30;
                min_z = point[2] - radius;
                stage = D_8018F481;
                keep_flags = ~0x20U;
                other_cursor = (RuntimeUnit **)(bank_address + 120U);
                max_z = point[2] + radius;
                for (; scan_index < 50; scan_index++, other_cursor++) {
                    u32 flags, next_flags;
                    /* Both real branch definitions retain the publication
                     * base's reference priority. These copies coalesce with
                     * entry; the native code has one joined flag store. */
                    RuntimeUnit *publish_entry;
                    entry = *other_cursor;
                    flags = entry->flags;
                    if (flags & 8) continue;
                    if ((flags & 0x31) == 0x11) {
                        f32 x = entry->floats_08.scalars.field_08;
                        f32 z;
                        f32 dx, dz;
                        if (!(min_x < x)) continue;
                        if (!(x < max_x)) continue;
                        z = entry->floats_08.scalars.field_10;
                        if (!(min_z < z)) continue;
                        if (!(z < max_z)) continue;
                        dx = x - point[0];
                        dz = z - point[2];
                        if (!(dx * dx + dz * dz < radius * radius)) continue;
                        next_flags = flags | 0x20;
                        publish_entry = entry;
                    } else {
                        if (stage != 0x3F) continue;
                        if ((flags & 0x01001000) != 0x01000000) continue;
                        next_flags = flags & keep_flags;
                        publish_entry = entry;
                    }
                    publish_entry->flags = next_flags;
                }
            }
    }
    func_0010ADB8(available);
}

/* Original physical order: func_00109A48. */
void func_00109A48(RuntimeUnit *unit)
{
    /* One actual working word holds flags, signed list values and promoted
     * row bytes in successive phases; none of those values overlap. */
    union { u32 flags; s32 value; } work;
    s32 scan_index = 30;
    RuntimeUnit **slot = D_801F0D28;

    /* Each route advances the real scan exactly once. The counted mark
     * jump keeps publication separate during delay-slot relaxation. */
    do {
        RuntimeUnit *entry = *slot;
        work.flags = entry->flags;
        if (work.flags & 8) goto first_advance_count;
        if (!(work.flags & 0x20)) goto first_advance_count;
        scan_index++;
        goto first_mark_flags;
first_advance_count:
        scan_index++;
        goto first_advance_cursor;
first_mark_flags:
        entry->flags = work.flags | 0x800;
first_advance_cursor:
        slot++;
    } while (scan_index < 50);

    {
        s32 index = 0;

        if (!(unit->flags & 0x40)) {
            if (unit->field_A8 != 0) {
                work.value = unit->field_A8[0];
                if (work.value != -1) {
                    RuntimeUnit **bank = D_801F0CB0;
                    u32 clear_mask;

                    scan_index = 0;
                    clear_mask = ~0x800U;
                    do {
                        bank[work.value]->flags &= clear_mask;
                        scan_index++;
                        work.value = unit->field_A8[scan_index];
                    } while (work.value != -1);
                }
            }
        } else {
            u8 (*rows)[11] = D_801969BA;
            s32 byte_sentinel = 0xFF;
            RuntimeUnit **bank = D_801F0CB0;
            s32 member_sentinel = -1;
            u32 clear_mask = ~0x800U;

            for (; index < 5; index++) {
                RuntimeUnit *entry;
                s32 terminator;

                /* The original repeats the call for each of the five bytes. */
                work.value = rows[func_0012E968(unit)][index];
                if (work.value == byte_sentinel) continue;
                entry = bank[work.value];
                if (entry->field_A8 == 0) continue;
                work.value = entry->field_A8[0];
                scan_index = 0;
                if (work.value == member_sentinel) continue;
                terminator = -1;
                do {
                    bank[work.value]->flags &= clear_mask;
                    scan_index++;
                    work.value = entry->field_A8[scan_index];
                } while (work.value != terminator);
            }
        }
    }
}

/* Original physical order: func_00109C04. */
void func_00109C04(void)
{
    s32 index = 30;
    /* The named initializer precedes pointer materialization in this compiler.
     * An embedded mask expression placed its constant after the pointer. */
    u32 clear_mask = ~0x800U;
    UnitFlags **slot = D_801F0D28;

    do {
        (*slot)->flags &= clear_mask;
        index++;
        slot++;
    } while (index < 50);
}

/* Original physical order: func_00109C3C. */
void func_00109C3C(RuntimeUnit *unit)
{
    f32 point[3];
    s32 index;
    f32 min_x, max_x, min_z;
    f32 point_x, point_z;
    /* The mask, setup address and bitmap test have disjoint real uses.
     * Sharing their word preserves the compiler's V1 preference. */
    union { u32 word; u8 *count; } work;

    unit->field_74 = -1;
    work.word = unit->flags & 0x00201011;
    if (work.word != 0x11) return;

    for (index = 0; index < D_801F1070; index++) {
            ResourceRecord *record = &D_801951B0[index];
            if (record->field_1C & 8) {
                f64 radius, center_x, center_z;
                f32 max_z;
                f32 x, z, dx, dz;

                func_0012EA80(index, point);
                radius = (f64)D_801F3A38 * D_801EE110;
                point_x = point[0];
                center_x = (f64)point_x;
                min_x = (f32)(center_x - radius);
                point_z = point[2];
                center_z = (f64)point_z;
                max_x = (f32)(center_x + radius);
                min_z = (f32)(center_z - radius);
                max_z = (f32)(center_z + radius);
                x = unit->floats_08.scalars.field_08;
                if (!(min_x < x)) goto next_record;
                if (!(x < max_x)) goto next_record;
                z = unit->floats_08.scalars.field_10;
                if (!(min_z < z)) goto next_record;
                if (!(z < max_z)) goto next_record;
                dx = x - point_x;
                dz = z - point_z;
                if (!((f64)(dx * dx + dz * dz) < radius * radius)) goto next_record;

                if (unit->flags & 8) {
                    if (record->field_1C & 2) unit->field_74 = index;
                    else goto query_record;
                } else if (record->field_1C & 4) {
                    unit->field_74 = index;
                } else {
query_record:
                    if (func_801DD244()) unit->field_74 = index;
                }

                if (unit->field_78 != 0) goto next_record;
                func_00118644(unit->source_index, index);
                if (func_801DD244() == 0) {
                    if (unit->flags & 8) {
                        if (!(record->field_1C & 2)) {
                            if (record->field_18 != 0) goto next_record;
                            if (func_0012DA9C(unit)) goto next_record;
                            if (!func_0012DA10(unit)) goto next_record;
                            if ((unit->flags & 0x00040000) || unit->field_20 != 0) {
                                if (unit->source_index < 30) goto next_record;
                            }
                            unit->field_7C = index;
                            unit->field_78 |= 0x40;
                            goto next_record;
                        }
                    } else {
                        goto other_record;
                    }
                }

                if ((unit->flags & 8) && func_0004813c(2)) {
                    u32 flags = unit->flags;
                    if (flags & 0x10000000) goto next_record;
                    if ((flags & 0x00040000) && unit->source_index < 30) goto next_record;
                    unit->field_7C = index;
                    unit->field_78 |= 0x00020000;
                } else {
other_record:
                    if (func_801DD244()) {
                        if (!(record->field_1C & 2)) goto next_record;
                    }
                    if (unit->flags & 8) goto next_record;
                    if (record->field_1C & 4) goto next_record;
                    if (record->field_18 != 0) goto next_record;
                    unit->field_7C = index;
                    unit->field_78 |= 0x00010000;
                }
            }
next_record:
            ;
    }

    if (unit->flags & 8) {
        if (func_801DD2B0(unit) == 0) {
            if (unit->field_78 == 0) {
                u8 *point_count;
                Bounds *bounds;
                f32 interpolation_divisor, radius_divisor;
                u32 status_bit, one;
                work.count = &D_801F0FE2;
                index = 0;
                if (index < *work.count) {
                    /* Retail derives this view from the count address minus 0x24A.
                     * Form it before retaining the count pointer for the backedge. */
                    bounds = (Bounds *)((u32)work.count - 0x24A);
                    /* These genuine loop constants keep the retained-count copy
                     * after their definitions in the scheduled preheader. */
                    interpolation_divisor = 256.0f;
                    radius_divisor = 2.0f;
                    status_bit = 0x00400000;
                    one = 1;
                    point_count = work.count;
                    do {
                        s32 value = D_801F1024[index];
                        s32 group = value / 8;
                        s32 bit = value - group * 8;
                        work.word = (D_80196A38[group] >> bit) & 1;
                        if (!work.word) {
                            f32 radius;
                            f32 max_z;
                            f32 unit_z;

                            point_x = bounds->field_00 + ((bounds->field_08 - bounds->field_00) * D_801F1004[index]) / interpolation_divisor;
                            point_z = bounds->field_04 + ((bounds->field_0C - bounds->field_04) * D_801F1014[index]) / interpolation_divisor;
                            radius = D_801F3A38 / radius_divisor;
                            min_x = point_x - radius;
                            max_x = point_x + radius;
                            min_z = point_z - radius;
                            max_z = point_z + radius;
                            point_x = unit->floats_08.scalars.field_08;
                            if (!(min_x < point_x)) goto next_point;
                            if (!(point_x < max_x)) goto next_point;
                            unit_z = unit->floats_08.scalars.field_10;
                            if (!(min_z < unit_z)) goto next_point;
                            if (!(unit_z < max_z)) goto next_point;
                            unit->field_7C = index;
                            unit->field_78 |= status_bit;
                            D_80196A38[group] |= one << bit;
                            break;
                        }
next_point:
                        ;
                    } while (++index < *point_count);
                }
            }
        }
    }
    if (unit->field_74 == -1) unit->flags &= ~0x10000000;
}

/* Original physical order: func_0010A128. */
s32 func_0010A128(RuntimeUnit *unit)
{
    s32 members[51];
    u8 first, second;
    s32 result = 0;
    s32 count;
    s32 lower;
    s32 index;
    s32 initial_index;
    f32 center_x, center_z, radius;
    f32 small_radius;
    f32 unit_x, unit_z;
    f32 min_x, min_z, max_x, max_z;
    f32 small_min_x, small_min_z, small_max_x, small_max_z;

    if (unit->field_A8 != 0) {
        resource_free(unit->field_A8);
        unit->field_A8 = 0;
    }
    unit->field_74 = -1;
    if (!(unit->flags & 1)) return 0;
    if (!(unit->flags & 0x10)) return 0;
    if ((unit->flags & 0x28) == 8) return 0;
    func_00109C3C(unit);
    members[0] = -1;
    count = 0;

    if (!(unit->flags & 0x00020000)) {
        f32 shift_x, shift_z;
        shift_x = D_801F3A38 * 1.5f;
        shift_x *= func_00020bf0(unit->field_18 * 6.2831855f);
        shift_z = D_801F3A38 * 1.5f;
        center_x = unit->floats_08.scalars.field_08 + shift_x;
        shift_z *= sinf(unit->field_18 * 6.2831855f);
        radius = D_801F3A38 * D_801EB2F0[unit->field_70];
        center_z = unit->floats_08.scalars.field_10 - shift_z;
    } else {
        center_x = unit->floats_08.scalars.field_08;
        center_z = unit->floats_08.scalars.field_10;
        radius = D_801F3A38 * 1.2f;
    }
    small_radius = D_801F3A38 / 2.0f;
    min_x = center_x - radius;
    max_x = center_x + radius;
    min_z = center_z - radius;
    max_z = center_z + radius;
    unit_x = unit->floats_08.scalars.field_08;
    small_min_x = unit_x - small_radius;
    small_max_x = unit_x + small_radius;
    unit_z = unit->floats_08.scalars.field_10;
    small_min_z = unit_z - small_radius;
    small_max_z = unit_z + small_radius;
    /* Pair the starting values to retain the original scan prologue. */
    if (unit->flags & 8) {
        lower = 30;
        initial_index = 50;
    } else {
        lower = 0;
        initial_index = 50;
    }

    for (index = initial_index - 1; index >= lower; index--) {
        RuntimeUnit *other = D_801F0CB0[index];
        s32 blocked = 0;
        f32 x, z, dx, dz;
        u32 flags, other_flags;

        if (lower != 0) {
            if (other->flags & 8) continue;
        } else {
            if (!(other->flags & 8)) continue;
        }
        if (D_801F361C != 0) {
            s32 stage = D_8018F481;
            if ((u8)stage == 0x27) {
                func_0013A558(D_801F361C[unit->field_14], D_801F361C[other->field_14], &first, &second);
                if (!first) blocked = 1;
            } else if ((u32)(stage - 0x2F) < 2 || (u32)(stage - 0x3C) < 2) {
                func_0013AA40(D_801F361C[unit->field_14], D_801F361C[other->field_14], &first, &second);
                if (!first) blocked = 1;
            } else if ((u8)stage == 0x33) {
                func_0013B350(D_801F361C[unit->field_14], D_801F361C[other->field_14], &first, &second);
                if (!first) blocked = 1;
            }
        }
        x = other->floats_08.scalars.field_08;
        if (!(min_x < x)) continue;
        if (!(x < max_x)) continue;
        z = other->floats_08.scalars.field_10;
        if (!(min_z < z)) continue;
        if (!(z < max_z)) continue;
        dx = x - center_x;
        dz = z - center_z;
        if (!(dx * dx + dz * dz < radius * radius)) continue;

        flags = unit->flags;
        if (flags & 8) {
            other_flags = other->flags;
            if (other_flags & 1) {
                if (other_flags & 0x10) {
                    /* Keep the guarded mask as a user scalar: this compiler
                     * hoists bare masks and changes the scan register layout. */
                    u32 mask_00200000 = 0x00200000;
                    if (!(flags & mask_00200000)) {
                        if (!(other_flags & mask_00200000)) {
                            if (small_min_x < x) {
                                if (x < small_max_x) {
                                    if (small_min_z < z) {
                                        if (z < small_max_z) {
                                            dx = x - unit_x;
                                            dz = z - unit_z;
                                            if (dx * dx + dz * dz < small_radius * small_radius) {
                                                if (!(flags & 0x1000)) {
                                                    if (!(other_flags & 0x1000)) {
                                                        if (unit->field_78 == 0 && D_801F0DE0 == 0) {
                                                            if (other->field_78 & 0x10) {
                                                                s32 owner;
                                                                for (owner = 0; owner < 50; owner++) {
                                                                    RuntimeUnit *candidate = D_801F0CB0[owner];
                                                                    if ((candidate->flags & 0x39) == 0x39) {
                                                                        if (candidate->field_78 & 0x10) {
                                                                            if (candidate->field_7C == index) break;
                                                                        }
                                                                    }
                                                                }
                                                                if (owner >= 50) other->field_78 &= ~0x10;
                                                                if (other->field_78 & 0x10) goto membership;
                                                            }
                                                            unit->field_7C = index;
                                                            unit->field_78 |= 0x10;
                                                            other->field_78 |= 0x10;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
membership:
        if (blocked) {
            if (!(unit->flags & 8)) continue;
            if (!(unit->field_78 & 0x10)) continue;
            if (unit->field_7C != index) continue;
        }
        members[count++] = index;
        members[count] = -1;
    }
    if (count != 0) {
        u32 size = (count + 1) * sizeof(s32);
        unit->field_A8 = resource_alloc(size);
        memcpy(members, unit->field_A8, size);
        result = 1;
    }
    return result;
}

/* Original physical order: func_0010A718. */
void func_0010A718(RuntimeUnit *unit)
{
    f32 point[3];
    s32 index;

    for (index = 0; index < D_801F0E18.count; index++) {
            u8 *record = D_801F0E19[index].bytes;
            u32 mask_11 = 0x11;
            /* The four-float bounds owner begins128 bytes before the
             * bank header. The original keeps its second float as the
             * address base; these are the same four genuine reads. */
            f32 *bounds_z = (f32 *)((u8 *)&D_801F0E18 - 124);
            s32 selected = 0;
            s32 comparison_index, current_value;
            /* Normalized Z and the case15 record-end address both use a1
             * in the original. One signed word keeps the casts multi-set
             * while the Z predicate remains a separate local result. */
            s32 compare_word;
            switch (record[1]) {
            case 3:
            case 7:
                if ((unit->flags & 0x11) == mask_11) {
                    f32 distance;
                    s32 kind;
                    func_0012EA80(record[6] - 1, point);
                    distance = hypotf(unit->floats_08.scalars.field_08 - point[0], unit->floats_08.scalars.field_10 - point[2]);
                    if (distance < D_801F3A38 / 2.0f) {
                        kind = record[1];
                        if ((u8)kind == 7 && (unit->flags & 8)) goto selected_record;
                        if ((u8)kind == 3 && !(unit->flags & 8)) selected = 1;
                    }
                }
                break;
            case 1:
            case 2:
            case 5:
            case 8:
                {
                    u32 flags = unit->flags;
                    if ((flags & 0x11) == mask_11) {
                        f32 z = unit->floats_08.scalars.field_10;
                        f32 base_z = bounds_z[0];
                        f32 base_x, width_x;
                        /* Single-assignment stages preserve this compiler
                         * scheduler's original X/width allocation. */
                        f32 x_load, x_offset, x_scaled, x;
                        s32 min_byte_z, max_byte_x, max_byte_z;
                        /* The X word also holds the later record selector,
                         * keeping the two conversions at equal birth status. */
                        s32 kind;
                        /* Two named booleans avoid the old frontend's
                         * conditional lowering of comparison & variable. */
                        s32 upper_x, lower_x;

                        z -= base_z;
                        z *= 256.0f;
                        z /= bounds_z[2] - base_z;
                        base_x = bounds_z[-1];
                        x_load = unit->floats_08.scalars.field_08;
                        x_offset = x_load - base_x;
                        width_x = bounds_z[1] - base_x;
                        x_scaled = x_offset * 256.0f;
                        x = x_scaled / width_x;
                        compare_word = (s32)z;
                        kind = (s32)x;
                        min_byte_z = record[3];
                        max_byte_x = record[4];
                        max_byte_z = record[5];
                        upper_x = max_byte_x >= kind;
                        lower_x = kind >= record[2];
                        lower_x &= upper_x;
                        if (lower_x) {
                            if ((compare_word >= min_byte_z) & (max_byte_z >= compare_word)) {
                                kind = record[1];
                                if (kind == 1 && (flags & 8)) goto selected_record;
                                if (kind == 2 && !(flags & 8)) goto selected_record;
                                if (kind == 5 && unit->source_index == record[6]) goto selected_record;
                                if (kind == 8 && unit->source_index == record[6]) selected = 1;
                            }
                        }
                    }
                }
                break;
            case 4:
                if ((unit->flags & 0x11) == mask_11) {
                    if (unit->flags & 8) {
                        comparison_index = record[6] - 1;
                        current_value = unit->field_74;
                        goto compare_record_value;
                    }
                }
                break;
            case 11:
                if ((unit->flags & 0x11) == mask_11) {
                    if (!(unit->flags & 8)) {
                        if (unit->field_74 == record[6] - 1) {
                            if (unit->source_index != 30) selected = 1;
                        }
                    }
                }
                break;
            case 6:
                if ((unit->flags & 0x11) != mask_11) break;
                if (record[6] == 1 && func_0005c110(0)) goto selected_record;
                if (record[6] == 6 && func_0005c110(1)) goto selected_record;
                if (record[6] == 7 && func_0005c110(2)) goto selected_record;
                if (record[6] == 8 && func_0005c110(3)) goto selected_record;
                if (record[6] == 9 && func_0005c110(4)) goto selected_record;
                if (record[6] == 10 && func_0005c110(5)) goto selected_record;
                if (record[6] == 11 && func_0005c110(6)) goto selected_record;
                if (record[6] == 12 && func_0005c110(7)) goto selected_record;
                if (record[6] == 13 && func_0005c110(8)) goto selected_record;
                if (record[6] == 14 && func_0005c110(9)) goto selected_record;
                if (record[6] == 15 && func_0005c110(10)) goto selected_record;
                if (record[6] == 16 && func_0005c110(11)) goto selected_record;
                if (record[6] == 17 && func_0005c110(12)) goto selected_record;
                if (record[6] == 18 && func_0005c110(13)) goto selected_record;
                if (record[6] == 23 && func_0005c110(14)) selected = 1;
                break;
            case 10:
                if (unit->source_index == record[6]) {
                    u32 flags = unit->flags;
                    if (!(flags & 0x10)) selected = 1;
                    else if ((flags & 0x01000008) == 0x01000000) selected = 1;
                }
                break;
            case 12:
                if ((unit->flags & 0x11) == mask_11) {
                    if (D_801951CC[record[6] - 1][0] & 4) selected = 1;
                }
                break;
            case 13:
                if ((unit->flags & 0x11) == mask_11) {
                    if (unit->flags & 8) {
                        comparison_index = record[6] - 1;
                        current_value = unit->field_74;
                        if (current_value == comparison_index) goto selected_record;
                        comparison_index = record[7] - 1;
                        /* Case4 enters this original shared comparison tail. */
compare_record_value:
                        if (current_value == comparison_index) selected = 1;
                    }
                }
                break;
            case 15:
                if ((unit->flags & 0x11) == mask_11) {
                    if (unit->flags & 8) {
                        u8 *entry = record + 2;
                        compare_word = (s32)(record + 10);
                        do {
                            if (*entry != 0) {
                                if (unit->field_74 == *entry - 1) goto selected_record;
                            }
                            entry++;
                        } while ((s32)entry < compare_word);
                    }
                }
                break;
            case 14:
                if ((unit->flags & 0x11) == mask_11) {
                    if (unit->flags & 8) {
                        s32 value = unit->field_74;
                        if ((u32)value - 1 < 2) selected = 1;
                        else if ((value == 3) | (value == 6)) selected = 1;
                    }
                }
                break;
            case 19:
                if ((unit->flags & 0x11) == mask_11) {
                    RuntimeUnit *other = D_801F0CB0[record[6]];
                    if (!(other->flags & 0x10)) selected = 1;
                    else if (func_0012DA10(other) == 0) selected = 1;
                }
                break;
            case 9:
            case 16:
            case 17:
            case 18:
            default:
                break;
selected_record:
                selected = 1;
                break;
            }
            if (selected) {
                u16 *published = &D_801F0EBC;
                /* Under the pinned MIPS compiler this uses the native
                 * variable shift, whose count is the low five bits. This
                 * retains the original modulo32 machine behavior. */
                *published |= 1U << (record[0] - 1);
            }
    }
}

/* Original physical order: func_0010ADB8. */
void func_0010ADB8(s32 available)
{
    u8 index = D_801E8680;

    while (index < D_801E8680 + 3) {
        if (index >= 50) break;
        func_0010A128(D_801F0CB0[index]);
        if (D_801F0E18.count != 0) {
            func_0010A718(D_801F0CB0[index]);
        }
        index++;
    }

    D_801E8680 += 3;
    if (D_801E8680 > 50) {
        D_801E8680 = 0;
        if (D_801F0E18.count != 0) {
            for (index = 0; index < D_801F0E18.count; index++) {
                u8 *record = D_801F0E18.records[index];
                /* Retail derives the mask from an unchecked loaded ID.
                 * This expression follows its C shift form; ID zero or an
                 * excessive shift has no defined C behavior. Static layout
                 * and a byte match do not establish the resource domain. */
                u16 mask = 1U << (record[0] - 1);

                if (record[1] == 9) D_801F0E18.primary &= ~mask;
                if (record[1] == 14) D_801F0E18.primary &= ~mask;
                if (record[1] == 12) D_801F0E18.primary &= ~mask;
                if (record[1] == 5) D_801F0E18.primary &= ~mask;

                if ((D_801F0E18.primary & mask) == 0) {
                    switch (record[1]) {
                    case 2:
                    case 3:
                        if ((D_801F0E18.secondary & mask) == 0) {
                            D_801F0E18.primary |= mask;
                        }
                        break;
                    case 9:
                        if (available != 0) {
                            if (record[6] >= available) {
                                D_801F0E18.primary |= mask;
                            }
                        }
                        break;
                    case 14:
                        /* Keep this positive gate and its own mask test.
                         * The pinned compiler gives the saved mask its
                         * retail allocation before merging the common tail. */
                        if ((available != 0) & (available < 7)) {
                            if (D_801F0E18.secondary & mask) {
                                D_801F0E18.primary |= mask;
                            }
                        }
                        break;
                    case 1:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 10:
                    case 12:
                    case 13:
                    case 19:
                        if (D_801F0E18.secondary & mask) {
                            D_801F0E18.primary |= mask;
                        }
                        break;
                    case 11:
                    case 15:
                    case 16:
                    case 17:
                    case 18:
                    default:
                        break;
                    }
                    if (record[1] != 14) D_801F0E18.secondary &= ~mask;
                    if (record[1] == 5) D_801F0E18.secondary &= ~mask;
                }
            }
        }
    }
}
