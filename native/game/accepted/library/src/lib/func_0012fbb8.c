typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

s32 func_00022e60(void);
extern u8 g_func_001957D0_source_records[];

s32 func_0012FBB8(s32 arg0, s32 arg1) {
    s32 average_value;
    s32 score;
    s32 penalty;
    s32 slots_end;
    s32 morale_value;
    s32 new_chaos_frame;
    s32 member_count;
    s32 random_value;
    s32 included_count;
    /* Keep the accumulated value narrow. KMC then computes the signed quotient
     * in a temporary and copies it back, preserving the retail division block. */
    u16 alignment;
    s32 alignment_gap;
    s32 work_value;
    s32 chaos_delta;
    s32 is_player_unit;
    u8 *slot;
    u8 member;
    u16 site_morale;
    u16 high_bit_adjustment;

    site_morale = *(u8 *)(0x801951CE + (arg1 * 0x24));
    if ((site_morale & 0xFFFF) == 0xFF) {
        goto reject;
    }
    if (arg0 < 0x1E) {
        goto accepted;
    }
reject:
    return 0;

accepted:
    high_bit_adjustment = (site_morale & 0x80) ? 0x28 : 0;
    site_morale &= 0x7F;
    alignment = 0;
    included_count = 0;
    is_player_unit = 1;
    slot = g_func_001957D0_source_records + 2 + (arg0 * 0x19);
    slots_end = (s32)(slot + 5);
loop:
    member = *slot;
    if ((member == 0) | (member >= 0x63U)) {
        goto next;
    }
    if (is_player_unit == 0) {
        goto alternate_table;
    }
    alignment += *(u8 *)(0x80193BDB + (member * 0x38));
    goto counted;
alternate_table:
    alignment += *(u8 *)(0x8019557B + (member * 0x34));
counted:
    included_count += 1;
next:
    slot += 1;
    member_count = included_count & 0xFF;
    if ((s32)slot < slots_end) {
        goto loop;
    }
    if (member_count != 0) {
        goto divide_average;
    }
    alignment = 0x32;
    goto average_ready;
divide_average:
    alignment = (s32)(alignment & 0xFFFF) / member_count;
average_ready:

    random_value = func_00022e60();
    work_value = random_value % 8;
    average_value = alignment & 0xFFFF;
    morale_value = site_morale & 0xFFFF;
    alignment_gap = average_value - morale_value;
    penalty = high_bit_adjustment & 0xFFFF;
    if (alignment_gap > 0) {
        goto absolute_ready;
    }
    alignment_gap = morale_value - average_value;
absolute_ready:
    score = alignment_gap + work_value + penalty;
    work_value = score & 0xFFFF;
    if ((u32)work_value >= 0x8D) {
        chaos_delta = -5;
    } else if ((u32)work_value >= 0x6F) {
        chaos_delta = -4;
    } else if ((u32)work_value >= 0x51) {
        chaos_delta = -3;
    } else if ((u32)work_value >= 0x33) {
        chaos_delta = -2;
    } else if ((u32)work_value >= 0x1F) {
        chaos_delta = -1;
    } else if ((u32)work_value >= 0x10) {
        chaos_delta = 0;
    } else {
        chaos_delta = 2;
        if ((u32)work_value >= 6) {
            chaos_delta = 1;
        }
    }
    new_chaos_frame = chaos_delta + *(u8 *)0x801936A9;
    if (new_chaos_frame >= 0x65) {
        new_chaos_frame = 0x64;
    } else {
        new_chaos_frame &= (s32)~new_chaos_frame >> 31;
    }
    *(u8 *)0x801936A9 = new_chaos_frame;
    return (u32)(score & 0xFFFF) >= 0x1F;
}
