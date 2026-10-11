typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

u8 func_00044130(u8, u8, s32);
s32 func_00044358(s32);
s8 func_0004440c(u8 *, u8, u8, s32, s32, s32, s32, s32, s32);
s32 func_000454e0(s32);
s32 func_0020144C(u8);
s32 func_0020C448(void *);
void *func_0020C478(u32);
s32 func_0020D434(s32);
s32 func_00233F94(void *);
s32 func_00237750(void *, void *, s32, s32, s32);
void func_0022EF50(void *record)
{
    u8 selected_code;
    s32 score_5d;
    s32 initial_code;
    void *other_record;
    s32 score_49;
    s32 score_39;
    s32 score_41;
    s32 score_51;
    s32 kind_range;
    s32 kind;
    s8 selected_kind;
    u32 table_index;
    u8 initial_selection;
    u8 special_selection;
    u8 chosen_code;
    u8 table_code;
    initial_selection = func_00044130(
        *(u8 *)((u8 *)record + 0x4B), *(u8 *)((u8 *)record + 0x4F),
        func_0020D434(*(s32 *)((u8 *)record + 0x58)) & 0xFF);
    selected_code = initial_selection;
    kind = func_00044358(initial_selection & 0xFF);
    if (*(s32 *)((u8 *)record + 0x4C) == 0xA4) {
        special_selection = func_00044130(
            *(u8 *)((u8 *)record + 0x4B), 0xA4U, (*(u8 *)0x801CE8F8 % 3) & 0xFF);
        selected_code = special_selection;
        kind = func_00044358(special_selection & 0xFF);
    }
    kind_range = kind - 3;
    if ((((u32) (kind_range & 0xFF) < 2U) | ((kind & 0xFF) == 5)) == 0) {
        goto unchanged;
    }
    initial_code = selected_code;
    if ((u32) (initial_code - 0x31) < 2U) {
        *(s8 *)((u8 *)record + 0x78) = 3;
        other_record = func_0020C478(func_00233F94(record));
        selected_code = 0x39;
        if (other_record != 0) {
            *(s32 *)((u8 *)record + 0x7C) = 0x39;
            score_39 = func_00237750(record, other_record, 0, 0, 2);
            *(s32 *)((u8 *)record + 0x7C) = 0x41;
            score_41 = func_00237750(record, other_record, 0, 0, 2);
            *(s32 *)((u8 *)record + 0x7C) = 0x49;
            score_49 = func_00237750(record, other_record, 0, 0, 2);
            *(s32 *)((u8 *)record + 0x7C) = 0x51;
            score_51 = func_00237750(record, other_record, 0, 0, 2);
            *(s32 *)((u8 *)record + 0x7C) = 0x5D;
            score_5d = func_00237750(record, other_record, 0, 0, 2);
            if ((score_39 >= score_41) & (score_39 >= score_49)) {
                if ((score_39 >= score_51) & (score_39 >= score_5d)) {
                    selected_code = 0x39;
                    goto selected;
                }
            }
            if ((score_41 >= score_49) & (score_41 >= score_51)) {
                if (score_41 >= score_5d) {
                    chosen_code = 0x41;
                    goto chosen;
                }
            }
            if ((score_49 >= score_51) & (score_49 >= score_5d)) {
                chosen_code = 0x49;
            } else if (score_51 >= score_5d) {
                /* Publishing directly in these final two arms preserves the
                 * retail branch. Joining them through chosen_code lets the
                 * pinned compiler replace it with arithmetic selection. */
                selected_code = 0x51;
                goto selected;
            } else {
                selected_code = 0x5D;
                goto selected;
            }
chosen:
            selected_code = chosen_code;
        }
    } else {
        initial_code &= 0xFF;
        if (initial_code == 0x91) {
            table_index = func_000454e0(func_0020C448(record) & 0xFFFF) & 0xFF;
            switch (table_index) {
            case 1:
                table_code = 0x93;
                break;
            case 2:
                table_code = 0x94;
                break;
            case 3:
                table_code = 0x95;
                break;
            case 4:
                table_code = 0x96;
                break;
            case 5:
                table_code = 0x97;
                break;
            case 6:
                table_code = 0x98;
                break;
            default:
            case 0:
                table_code = 0x92;
                break;
            }
            selected_code = table_code;
            selected_kind = 4;
        } else {
            selected_kind = func_0004440c(
                &selected_code, *(u8 *)((u8 *)record + 0x4B),
                *(u8 *)((u8 *)record + 0x4F), initial_code,
                (s32)*(u8 *)((u8 *)record + 0x33),
                (s32)*(u16 *)((u8 *)record + 0x36),
                (s32)*(u16 *)((u8 *)record + 0x38),
                (s32)*(u16 *)((u8 *)record + 0x3A),
                (s32)*(u16 *)((u8 *)record + 0x3C));
        }
        *(s8 *)((u8 *)record + 0x78) = selected_kind;
    }
selected:
    *(s32 *)((u8 *)record + 0x7C) = (s32) selected_code;
    /* Retail reloads this byte at the common publication tail. The local
     * volatile read preserves that observed load instead of reusing its value. */
    *(s32 *)((u8 *)record + 0x80) =
        (s32)(func_0020144C(*(volatile u8 *)&selected_code) & 0xFF);
    return;
unchanged:
    *(s8 *)((u8 *)record + 0x78) = kind;
    kind_range = selected_code;
    *(s32 *)((u8 *)record + 0x80) = -1;
    *(s32 *)((u8 *)record + 0x7C) = kind_range;
    return;
}
