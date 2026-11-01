typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

s32 func_00041638(u16);
s32 func_00043100(u8, u8);

extern u8 D_80193AC0[];
extern u8 D_80193BC0[];

void func_000bcee4(s32 character_slot)
{
    register s32 total asm("$17");
    s32 saved_total;
    register s32 index asm("$16");
    s32 record_stride;
    s32 equipment_offset;
    s32 item_offset;
    s32 record_offset;
    s32 final_offset;
    s32 sentinel;
    u8 *class_table;
    u8 *equipment_table;
    register u8 *data asm("$2");
    register u8 *final_data asm("$4");
    u8 *record;
    register u8 *read asm("$4");
    register u8 *write asm("$3");
    u8 *equipment_cursor;
    u8 *character;
    u32 item;
    u8 class_a;
    u8 class_b;
    u8 equipment_count;
    u16 value;

    total = 0;
    index = 0;
    record_stride = (character_slot & 0xFF) * 54;
    sentinel = 0xFF;
    class_table = D_80193BC0;
    record_offset = (character_slot & 0xFFFF) * 54;
    record_offset += 0x117C;
    record = *(u8 **)0x80196AF8 + record_offset;

scan_loop:
    data = *(u8 **)0x80196AF8;
    data += record_stride;
    data += index;
    item = data[0x1180];
    asm volatile(
        "# Hybrid scope: this branch-likely skips invalid 0xFF entries in the nine-slot scan.\n"
        "# If the item equals the sentinel, its delay slot increments the scan index and branches to the C loop test.\n"
        "# If unequal, MIPS nullifies the delay slot and the following C path performs the sole increment before its call.\n"
        "# KMC lowers the equivalent structured C to `bne` plus a local jump instead of retail's two-instruction `beql`.\n"
        "# The input constraint emits no instruction; it keeps the $s6 sentinel live for this fragment.\n"
        "# Keep the branch-likely delay slot exactly where written.\n"
        ".set noreorder\n"
        "beql $4,$22,.Lbcee4_scan_test # branch when the scanned item is the 0xFF sentinel\n"
        "addiu $16,$16,1 # increment the nine-slot scan index only on the taken likely branch\n"
        "# Restore normal assembler scheduling after the fixed branch pair.\n"
        ".set reorder\n"
        : : "r" (sentinel));
    {
        register s32 item_is_zero asm("$3");
        register s32 item_is_out_of_range asm("$2");

        asm volatile(
            "# Hybrid scope: these four instructions form the invalid-item predicate in retail order.\n"
            "# $v1 becomes (item == 0); $v0 becomes (item >= 100) through an unsigned compare and inversion.\n"
            "# Their OR is retained in $v1; the following C branch selects default class pair (1,1) when nonzero.\n"
            "# KMC schedules the comparisons differently and chooses $v0 for the combined result.\n"
            "sltiu %0,$4,1 # set item_is_zero when the unsigned item value is zero\n"
            "sltiu %1,$4,0x64 # set the temporary when the unsigned item is below 100\n"
            "xori %1,%1,1 # invert that temporary so it means item is at least 100\n"
            "or %0,%0,%1 # combine zero and out-of-range into the invalid-item predicate\n"
            : "=r" (item_is_zero), "=r" (item_is_out_of_range));
        class_b = 1;
        if (item_is_zero != 0) {
            class_a = 1;
        } else {
            character = (u8 *)(item * 56 + (u32)class_table);
            class_a = character[0x11];
            class_b = character[0x12];
        }
    }
    index += 1;
    total += func_00043100(class_a, class_b) & 0xFF;
    asm volatile(
        "# Hybrid scope: this zero-byte local label is the invalid-entry branch target and starts the C loop test.\n"
        ".Lbcee4_scan_test:");
    read = record;
    if (index < 9) {
        goto scan_loop;
    }

    asm volatile(
        "# Hybrid scope: these two moves begin the compaction phase in retail order.\n"
        "# $s5 preserves the completed eligibility count from $s1, then $s0 resets the loop index to zero.\n"
        "# KMC reverses these independent C assignments; their values and all subsequent loop logic remain C.\n"
        "move %0,%2 # preserve the completed eligibility total for the final record field\n"
        "move %1,$0 # reset the compaction loop index to zero\n"
        : "=r" (saved_total), "=r" (index) : "r" (total));
    asm volatile(
        "# Hybrid scope: this `move` copies the record base from $s2 into compaction write cursor $v1.\n"
        "# Read cursor $a0 already holds the same address, but retail preserves a distinct copy and KMC coalesces it.\n"
        "move %0,$18 # copy the character-record base into the compaction write cursor\n"
        : "=r" (write));
compact_loop:
    value = *(u16 *)(read + 0x22);
    index += 1;
    if (value != 0) {
        *(u16 *)(write + 0x22) = value;
        write += 2;
    }
    read += 2;
    if (index < 10) {
        goto compact_loop;
    }

    asm volatile(
        "# Hybrid scope: this `andi` copies the saved count from $s5 directly into loop index $s0.\n"
        "# KMC otherwise routes the value through a caller-saved temporary and emits an extra `move`.\n"
        "andi %0,%1,0xFFFF # copy the saved eligibility count into the zero-extended fill index\n"
        : "=r" (index) : "r" (saved_total));
    if (index < 10) {
        equipment_table = D_80193AC0;
        record_stride = (character_slot & 0xFFFF) * 54;
        equipment_cursor = (u8 *)(index * 2 + (u32)record);
        do {
            value = *(u16 *)(equipment_cursor + 0x22);
            item_offset = index * 2;
            if (value != 0) {
                equipment_offset = (func_00041638(value) & 0xFFFF) * 4;
                equipment_count = D_80193AC0[equipment_offset + 2];
                equipment_table[equipment_offset + 2] =
                    equipment_count - 1;
                data = *(u8 **)0x80196AF8;
                data += record_stride;
                data = (u8 *)(item_offset + (u32)data);
                *(u16 *)(data + 0x119E) = 0;
            }
            index += 1;
            equipment_cursor += 2;
        } while (index < 10);
    }

    final_data = *(u8 **)0x80196AF8;
    final_offset = (character_slot & 0xFFFF) * 54;
    final_data += final_offset;
    final_data[0x119C] = saved_total;
    asm(
        "# Hybrid scope: the register-asm declarations above emit no instructions.\n"
        "# They keep the scan total/index in $s1/$s0, data reloads in $v0, final data in $a0,\n"
        "# compaction read/write cursors in $a0/$v1, and predicate results in $v1/$v0.\n"
        "# All table walks, calls, equipment-count updates, record clears, and final stores remain C.\n");
}
