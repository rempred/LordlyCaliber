typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;

typedef struct {
    u32 flags;
    u8 source_index;
    u8 pad05[0x6F];
    s32 field74;
    u8 pad78[8];
    s32 field80;
    s32 field84;
    u8 pad88[9];
    u8 field91;
    u8 field92;
} RuntimeUnit;
typedef struct { u8 bytes[11]; } DeploymentRow11;

extern DeploymentRow11 D_801969B8[];
extern u8 D_801969BA[][11];
extern u8 D_801971F1[][25];
extern RuntimeUnit *D_801F0CB0[];
extern void func_0010746C(RuntimeUnit *unit);
extern u8 func_001291B4(u8 *row);
extern s32 func_0012E968(RuntimeUnit *unit);

static inline s32 row_index_for_unit(RuntimeUnit *unit)
{
    u32 source_index = unit->source_index;
    s32 row_index;
    for (row_index = 0; row_index < 10; row_index++) {
        DeploymentRow11 *row = &D_801969B8[row_index];
        u32 flags = row->bytes[1];
        s32 slot;
        if (!(flags & 1)) {
            continue;
        }
        if (unit->flags & 8) {
            if (flags & 0x80) {
                continue;
            }
        } else if (!(flags & 0x80)) {
            continue;
        }
        for (slot = 0; slot < 5; slot++) {
            if (row->bytes[slot + 2] == source_index) {
                return row_index;
            }
        }
    }
    return -1;
}

static inline s32 slot_index_for_unit(RuntimeUnit *unit)
{
    u32 source_index = unit->source_index;
    s32 row_index;
    for (row_index = 0; row_index < 10; row_index++) {
        DeploymentRow11 *row = &D_801969B8[row_index];
        u32 flags = row->bytes[1];
        s32 slot;
        if (!(flags & 1)) {
            continue;
        }
        if (unit->flags & 8) {
            if (flags & 0x80) {
                continue;
            }
        } else if (!(flags & 0x80)) {
            continue;
        }
        for (slot = 0; slot < 5; slot++) {
            if (row->bytes[slot + 2] == source_index) {
                return slot;
            }
        }
    }
    return -1;
}

void func_0012F140(RuntimeUnit *unit)
{
    if (unit->flags & 0x40) {
        s32 row_index = row_index_for_unit(unit);
        DeploymentRow11 *row;
        s32 slot;
        slot_index_for_unit(unit);
        row = &D_801969B8[row_index];
        if (unit->flags & 0x80) {
            row->bytes[1] &= ~1;
            D_801971F1[unit->source_index][0] &= ~2;
            for (slot = 0; slot < 5; slot++) {
                u32 source_index = row->bytes[slot + 2];
                if (source_index != 0xFF) {
                    RuntimeUnit *member = D_801F0CB0[source_index];
                    func_0010746C(member);
                    member->field92 = 0;
                    member->field91 = 0;
                    member->field84 = -1;
                    member->field80 = -1;
                    member->flags &= ~0x80;
                    member->flags &= ~0x40;
                    member->flags &= ~0x4000;
                    member->flags &= ~0x8000;
                    member->flags &= ~0x10000;
                    member->flags &= ~0x800000;
                    if (slot != 0 && !(member->flags & 8)) {
                        member->flags |= 0x20000000;
                    }
                }
            }
            if (unit->flags & 8) {
                unit->flags |= 0x02000000;
            }
        } else {
            func_0010746C(unit);
            /* Retain the same reset-store order as the member path; KMC uses
               it to break a flags-load/mask scheduling tie. */
            unit->field92 = 0;
            unit->field91 = 0;
            unit->field84 = -1;
            unit->field80 = -1;
            unit->flags &= ~0x80;
            unit->flags &= ~0x40;
            unit->flags &= ~0x4000;
            unit->flags &= ~0x8000;
            unit->flags &= ~0x10000;
            unit->flags &= ~0x800000;
            if (unit->flags & 8) {
                unit->flags |= 0x02000000;
            }
            for (slot = 0; slot < 5; slot++) {
                if (row->bytes[slot + 2] == unit->source_index) {
                    s32 shift_slot;
                    for (shift_slot = slot; shift_slot < 5; shift_slot++) {
                        if (shift_slot == 4) {
                            row->bytes[6] = 0xFF;
                        } else {
                            row->bytes[shift_slot + 2] = row->bytes[shift_slot + 3];
                        }
                    }
                    break;
                }
            }
        }
        row->bytes[9] = func_001291B4(row->bytes);
        {
            s32 new_row_index = func_0012E968(D_801F0CB0[row->bytes[2]]);
            s32 count = 0;
            const u8 *cursor = D_801969BA[new_row_index];
            const u8 *end = cursor + 5;
            do {
                count += *cursor++ != 0xFF;
            } while ((s32)cursor < (s32)end);
            row->bytes[10] = 11 - count;
        }
    }
}
