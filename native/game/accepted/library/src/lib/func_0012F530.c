typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;

typedef struct {
    u32 flags;
    u8 source_index;
    u8 pad05[0x6F];
    s32 field74;
} RuntimeUnit;
typedef struct { u8 bytes[11]; } DeploymentRow11;
extern DeploymentRow11 D_801969B8[];
extern RuntimeUnit *D_801F0CB0[];
extern u16 D_801951CC[][18];
extern u8 D_801971F2[][25];
extern u16 D_80193BD8[][28];
extern u16 D_80195578[][26];
extern u8 D_80193BF3[][56];
extern u8 D_80195593[][52];

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

static inline s32 first_record_present(RuntimeUnit *unit)
{
    u32 source_index = unit->source_index;
    s32 member = D_801971F2[source_index][0];
    if ((member == 0) | (member >= 100)) {
        return 0;
    }
    if (source_index < 30) {
        return D_80193BD8[member][0] != 0;
    }
    return D_80195578[member][0] != 0;
}

static inline s32 first_record_flag4(RuntimeUnit *unit)
{
    u32 source_index = unit->source_index;
    s32 member = D_801971F2[source_index][0];
    s32 flags;
    if ((member == 0) | (member >= 100)) {
        return 0;
    }
    if (source_index < 30) {
        flags = D_80193BF3[member][0] & 4;
    } else {
        flags = D_80195593[member][0] & 4;
    }
    return flags != 0;
}

s32 func_0012F530(RuntimeUnit *unit)
{
    s32 result;
    if (unit->flags & 0x40) {
        DeploymentRow11 *row = &D_801969B8[row_index_for_unit(unit)];
        s32 slot;
        for (slot = 0; slot < 5; slot++) {
            u32 source_index = row->bytes[slot + 2];
            if (source_index != 0xFF && (D_801F0CB0[source_index]->flags & 0x00020000)) {
                break;
            }
        }
        if (slot < 5) {
            result = 4;
        } else {
            for (slot = 0; slot < 5; slot++) {
                u32 source_index = row->bytes[slot + 2];
                if (source_index != 0xFF) {
                    s32 field74 = D_801F0CB0[source_index]->field74;
                    if (field74 != -1 && (D_801951CC[field74][0] & 0x10)) {
                        break;
                    }
                }
            }
            if (slot < 5) {
                for (slot = 0; slot < 5; slot++) {
                    if (row->bytes[slot + 2] != 0xFF && D_801971F2[unit->source_index][0] == 1) {
                        break;
                    }
                }
                if (slot < 5) {
                    result = 3;
                } else {
                    result = 6;
                }
            } else {
                result = 3;
            }
        }
    } else {
        if (!first_record_present(unit)) {
            result = 9;
        } else if (first_record_flag4(unit)) {
            result = 9;
        } else if (unit->flags & 0x00020000) {
            result = 2;
        } else {
            /* A value-producing expression keeps the retail shared return. */
            result = unit->field74 == -1 ? 1 :
                !(D_801951CC[unit->field74][0] & 0x10) ? 1 :
                D_801971F2[unit->source_index][0] == 1 ? 1 : 5;
        }
    }
    return result;
}
