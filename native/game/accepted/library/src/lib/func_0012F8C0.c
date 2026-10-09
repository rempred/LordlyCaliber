typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;

typedef struct { u32 flags; u8 source_index; } RuntimeUnit;
typedef struct { u8 bytes[11]; } DeploymentRow11;

extern DeploymentRow11 D_801969B8[];
extern u8 D_801969BB[][11];
extern u8 D_801969BE[][11];
extern u8 D_801969C2[][11];
extern u8 D_801971F2[][25];
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

static inline s32 count_row_slots(RuntimeUnit *unit)
{
    s32 row_index = func_0012E968(unit);
    /* Start the counter after the query, before forming the scan pointers. */
    s32 count = 0;
    u8 *slot = D_801969B8[row_index].bytes + 2;
    u8 *end = slot + 5;
    do {
        count += *slot != 0xFF;
        slot++;
    } while ((s32)slot < (s32)end);
    return count;
}

void func_0012F8C0(RuntimeUnit *unit)
{
    s32 row_index = row_index_for_unit(unit);
    s32 present = 0;
    s32 high_members = 0;
    u8 *slot = D_801969BB[row_index];
    u8 *member_base = D_801971F2[unit->source_index];
    u8 *member = member_base + 1;
    u8 *end = member_base + 5;
    s32 remaining;

    do {
        u32 member_id;
        present += *slot != 0xFF;
        member_id = *member;
        high_members += member_id >= 100;
        member++;
        slot++;
    } while ((s32)member < (s32)end);

    remaining = present - high_members;
    if (remaining > 0) {
        u32 sentinel = 0xFF;
        RuntimeUnit **owners = D_801F0CB0;
        u8 *cursor = D_801969BE[row_index];
        /* Keep this explicit back edge: a do/while lets KMC hoist four
           mask constants across the callback and grow the saved-register frame. */
clear_next_slot:
        {
            u32 source_index = *cursor;
            if (source_index != sentinel) {
                RuntimeUnit *removed = owners[source_index];
                func_0010746C(removed);
                removed->flags &= ~0x80;
                removed->flags &= ~0x40;
                removed->flags &= ~0x4000;
                removed->flags &= ~0x8000;
                removed->flags &= ~0x10000;
                removed->flags &= ~0x800000;
                if (!(removed->flags & 8)) {
                    removed->flags |= 0x20000000;
                }
                *cursor = sentinel;
                remaining--;
            }
            cursor--;
        }
        if (remaining > 0) goto clear_next_slot;
    }
    D_801969B8[row_index].bytes[9] = func_001291B4(D_801969B8[row_index].bytes);
    D_801969C2[row_index][0] =
        11 - count_row_slots(D_801F0CB0[D_801969B8[row_index].bytes[2]]);
}
