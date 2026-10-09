typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;

extern u8 D_8018F481;
extern u8 D_801974E0[];
extern u8 D_801971F2[][25];
extern u8 D_801971F1[][25];
extern u16 D_8019532C[];
extern u16 D_80190EBC[];
extern u8 D_80195480[];
extern u8 D_80191010[];
extern u16 D_80195578[][26];
extern u16 D_80193BD8[][28];
extern u8 D_80195593[][52];
extern u8 D_80193BF3[][56];

s32 func_0012ECDC(s32 source_index)
{
    s32 result;
    s32 first_group;
    /* Share the first-member temporary between the two special cases;
    keep their clear cursors separate from the later scan. */
    u32 first;
    u8 *cursor;
    u8 *start;
    u8 *end;

    /* Pointer limits below retain the retail signed 32-bit comparisons. */
    if ((u32)(D_8018F481 - 45) >= 2) {
        result = 0;
        if (source_index == 30) {
            first = D_801974E0[0];
            if (D_80195578[first][0] == 0 || (D_80195593[first][0] & 4)) {
                u8 *clear_cursor;
                u8 *clear_end;
                u16 *extra = D_8019532C;
                u8 *base = D_801971F2[0];
                clear_cursor = base + 30 * 25;
                clear_end = base + 30 * 25 + 5;
                do {
                    s32 member = *clear_cursor;
                    if (member != 0) {
                        if (member >= 100) {
                            extra[member] = 0;
                        } else {
                            D_80195578[member][0] = 0;
                        }
                    }
                    clear_cursor++;
                } while ((s32)clear_cursor < (s32)clear_end);
                return 0;
            }
        }
    } else {
        if ((u32)(source_index - 30) < 2) {
            s32 source_offset = source_index * 25;
            first = *(u8 *)((u32)D_801971F2 + source_offset);
            if (D_80195578[first][0] == 0 || (D_80195593[first][0] & 4)) {
                u8 *clear_cursor;
                u8 *clear_end;
                u16 *extra = D_8019532C;
                clear_cursor = (u8 *)((u32)D_801971F2 + source_offset);
                clear_end = clear_cursor + 5;
                do {
                    s32 member = *clear_cursor;
                    if (member != 0) {
                        if (member >= 100) {
                            extra[member] = 0;
                        } else {
                            D_80195578[member][0] = 0;
                        }
                    }
                    clear_cursor++;
                } while ((s32)clear_cursor < (s32)clear_end);
                return 0;
            }
        }
        result = 0;
    }

    first_group = source_index < 30;
    {
        u16 *extra_first = D_80190EBC;
        u16 *extra_second = D_8019532C;
        s32 source_offset = source_index * 25;
        cursor = (u8 *)((u32)D_801971F2 + source_offset);
        start = cursor;
        end = cursor + 5;
        do {
            s32 member = *cursor;
            if (member != 0) {
                if (member >= 100) {
                    if (first_group) {
                        if (extra_first[member] != 0) {
                            result = 1;
                        } else {
                            D_80191010[member] &= ~4;
                        }
                    } else {
                        if (extra_second[member] != 0) {
                            result = 1;
                        } else {
                            D_80195480[member] &= ~4;
                        }
                    }
                } else if (first_group) {
                    if (D_80193BD8[member][0] != 0) {
                        result = 1;
                    } else {
                        D_80193BF3[member][0] &= ~4;
                    }
                } else {
                    if (D_80195578[member][0] != 0) {
                        result = 1;
                    } else {
                        D_80195593[member][0] &= ~4;
                        if (cursor == start && !(*(u8 *)((u32)D_801971F1 + source_offset) & 0x80)) {
                            return 0;
                        }
                    }
                }
            }
            cursor++;
        } while ((s32)cursor < (s32)end);
        return result;
    }
}
