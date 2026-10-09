typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;

typedef struct { u8 bytes[25]; } SourceRecord25;
typedef struct { u8 bytes[11]; } DeploymentRow11;
typedef struct {
    u32 high : 24;
    u32 b7 : 1;
    u32 b6 : 1;
    u32 b5 : 1;
    u32 b4 : 1;
    u32 b3 : 1;
    u32 b2 : 1;
    u32 b1 : 1;
    u32 b0 : 1;
} UnitFlagBits;
typedef struct {
    union {
        u32 word;
        UnitFlagBits bits;
    } flags;
    u8 source_index;
} RuntimeUnit;

extern s32 D_801F367C;
extern RuntimeUnit *D_801F0CB0[];
extern DeploymentRow11 D_801969B8[];
extern SourceRecord25 g_func_001957D0_source_records[];
extern u8 D_801971F1[][25];
extern u8 func_00129068(u8 *record);
extern s32 func_00040f88(s32, s32, s32, s32, s32);

void func_00129268(void)
{
    s32 row_index;

    for (row_index = 0; row_index < D_801F367C; row_index++) {
        RuntimeUnit *unit = D_801F0CB0[row_index];
        if (unit->flags.bits.b4) {
            if (!unit->flags.bits.b0) {
                unit->flags.bits.b6 = 0;
                unit->flags.bits.b7 = 0;
                D_801971F1[unit->source_index][0] &= ~2;
            }
        }
    }

    row_index = 0;
    do {
        DeploymentRow11 *row = &D_801969B8[row_index];
        /* One real counter is reused by the mutually exclusive unit scans.
         * Sharing its scope preserves the pinned compiler's register allocation. */
        s32 unit_index;
        u8 values[5];
        s32 slot;

        if (row->bytes[1] & 1) {

            for (slot = 0; slot < 5; slot++) {
                u8 member_id = row->bytes[slot + 2];
                if (member_id == 0xFF) {
                    values[slot] = 0;
                } else {
                    values[slot] = func_00129068(g_func_001957D0_source_records[member_id].bytes);
                }
            }
            row->bytes[9] = func_00040f88(values[0], values[1], values[2],
                                           values[3], values[4]);

            if (!(row->bytes[1] & 0x80)) {
                register s32 scan_slot;
                for (scan_slot = 0; scan_slot < 5; scan_slot++) {
                    s32 member_id = row->bytes[scan_slot + 2];
                    if (member_id == 0xFF) {
                        continue;
                    }
                    for (unit_index = 0; unit_index < D_801F367C; unit_index++) {
                        RuntimeUnit *unit = D_801F0CB0[unit_index];
                        u32 flags = unit->flags.word;
                        if (!(flags & 8) || unit->source_index != member_id) {
                            continue;
                        }
                        unit->flags.word = flags | 0x40;
                        if (scan_slot == 0) {
                            unit->flags.word = flags | 0xC0;
                            D_801971F1[unit->source_index][0] |= 2;
                            if (D_801971F1[unit->source_index][0] & 4) {
                                row->bytes[1] |= 4;
                            } else {
                                row->bytes[1] &= ~4;
                            }
                        } else {
                            D_801971F1[unit->source_index][0] &= ~2;
                        }
                        break;
                    }
                }
            } else {
                register s32 scan_slot;
                for (scan_slot = 0; scan_slot < 5; scan_slot++) {
                    s32 member_id = row->bytes[scan_slot + 2];
                    if (member_id == 0xFF) {
                        continue;
                    }
                    for (unit_index = 0; unit_index < D_801F367C; unit_index++) {
                        RuntimeUnit *unit = D_801F0CB0[unit_index];
                        u32 flags = unit->flags.word;
                        if ((flags & 8) || unit->source_index != member_id) {
                            continue;
                        }
                        unit->flags.word = flags | 0x40;
                        if (scan_slot == 0) {
                            unit->flags.word = flags | 0xC0;
                            D_801971F1[unit->source_index][0] |= 2;
                            if (D_801971F1[unit->source_index][0] & 4) {
                                row->bytes[1] |= 4;
                            } else {
                                row->bytes[1] &= ~4;
                            }
                        } else {
                            D_801971F1[unit->source_index][0] &= ~2;
                        }
                        break;
                    }
                }
            }
        }
        row_index++;
    } while (row_index < 10);
}
