typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

struct StackData {
    u16 values[8];
};

typedef s32 (*ClassCheck)();

s32 func_000415fc(s32);
void func_00023780(void *, s32);
extern u8 D_80193BC0[];
extern ClassCheck D_801EF288[];

s32 func_000bf9c8(s32 arg0, s32 arg1)
{
    struct StackData stack_data;
    register s32 slot_or_item asm("$16");
    register s32 index_or_result asm("$17");
    s32 record_or_function_cursor;
    s32 offset_or_count;
    s32 three_or_total;
    s32 one_or_selected_class;
    register s32 table_or_sentinel asm("$23");
    register s32 two_or_value_base asm("$22");
    s32 first_selector;
    s32 selector;
    s32 mapped_item;
    register s32 matched asm("$3");
    register s32 sum_offset asm("$2");
    register u32 arg3 asm("$7");
    u8 class_a;
    u8 class_b;
    u16 first_value;

    func_00023780(&stack_data, 8);

    asm volatile(
        "# Hybrid scope: these three instructions preserve retail's always-enabled value-builder branch.\n"
        "# $v0 is set to one, so the branch to the second phase is never taken; its delay slot masks arg0 into the C slot cursor.\n"
        "# KMC constant-folds the equivalent C condition and deletes the load and branch, while retaining only the mask.\n"
        "# Preserve the never-taken branch and its explicit delay slot.\n"
        ".set noreorder\n"
        "addiu $2,$0,1 # put the constant one in $v0, making the following zero test false\n"
        "beq $2,$0,.Lbf9c8_second_phase # retain retail's never-taken branch around the value-building phase\n"
        "andi %0,%0,0xFF # zero-extend arg0 into the first-phase slot cursor in the branch delay slot\n"
        "# Restore normal assembler scheduling after the fixed branch.\n"
        ".set reorder\n"
        : "=r" (slot_or_item) : "0" (arg0));
    offset_or_count = slot_or_item * 0x38;
    table_or_sentinel = (s32)D_80193BC0;
    record_or_function_cursor = offset_or_count + table_or_sentinel;
    asm volatile(
        "# Hybrid scope: these five instructions initialize the first-phase constants and cursors in retail order.\n"
        "# $s6/$s5/$s4 receive selectors 2/1/3, $s0 receives the stack value-array base, and $s1 receives loop index one.\n"
        "# KMC emits the same C values but schedules the independent stack cursor and index before the three selector constants.\n"
        "addiu %0,$0,2 # initialize the selector-two constant in $s6\n"
        "addiu %1,$0,1 # initialize the selector-one constant in $s5\n"
        "addiu %2,$0,3 # initialize the selector-three constant in $s4\n"
        "addiu %3,$sp,0x10 # point $s0 at the eight-halfword local value array\n"
        "addiu %4,$0,1 # initialize the first-phase selector index to one in $s1\n"
        : "=r" (two_or_value_base), "=r" (one_or_selected_class),
          "=r" (three_or_total), "=r" (slot_or_item),
          "=r" (index_or_result));
    do {
        first_selector = index_or_result & 0xFF;
        if (first_selector == two_or_value_base) {
            goto first_case_2;
        }
        if (first_selector >= 3) {
            goto first_at_least_3;
        }
        if (first_selector == one_or_selected_class) {
            goto first_case_1;
        }
        goto first_default;
first_at_least_3:
        if (first_selector == three_or_total) {
            goto first_case_3;
        }
        goto first_default;
first_case_1:
        first_value = *(u16 *)(record_or_function_cursor + 0x2A);
        first_selector = index_or_result;
        goto first_selected;
first_case_2:
        first_value = *(u16 *)(record_or_function_cursor + 0x2C);
        first_selector = index_or_result;
        goto first_selected;
first_case_3:
        first_value = *(u16 *)(record_or_function_cursor + 0x2E);
        first_selector = index_or_result;
        goto first_selected;
first_default:
        first_value = *(u16 *)(record_or_function_cursor + 0x30);
        first_selector = index_or_result;
first_selected:
        arg3 = offset_or_count + table_or_sentinel;
        *(volatile u16 *)slot_or_item = first_value;
        class_a = ((u8 *)arg3)[0x11];
        class_b = ((u8 *)arg3)[0x12];
        asm volatile(
            "# Hybrid scope: this zero-byte barrier keeps the two class-byte loads before the following selector mask.\n"
            "# The input constraints emit no instructions; they only preserve retail's argument-load schedule.\n"
            : : "r" (class_a), "r" (class_b));

        selector = first_selector & 0xFF;
        if (selector == two_or_value_base) {
            goto second_case_2;
        }
        if (selector >= 3) {
            goto second_at_least_3;
        }
        if (selector == one_or_selected_class) {
            goto second_case_1;
        }
        goto second_default;
second_at_least_3:
        if (selector == three_or_total) {
            goto second_case_3;
        }
        goto second_default;
second_case_1:
        arg3 = *(u16 *)(arg3 + 0x2A);
        goto second_selected;
second_case_2:
        arg3 = *(u16 *)(arg3 + 0x2C);
        goto second_selected;
second_case_3:
        arg3 = *(u16 *)(arg3 + 0x2E);
        goto second_selected;
second_default:
        arg3 = *(u16 *)(arg3 + 0x30);
second_selected:
        selector = first_selector & 0xFF;
        if (((arg3 & 0xFFFF) == 0) &&
            ((selector != 0) & ((u32)selector < 5U))) {
            arg3 = (*(s32 (**)(u8, u8, u32, u32))
                (0x801EF284 + (selector * 4)))(
                    class_a, class_b, selector, arg3);
        }
        asm volatile(
            "# Hybrid scope: these five instructions store the generated value and remove an unchanged duplicate.\n"
            "# $v1 loads the original halfword, $v0 masks generated $a3, and the branch always stores that value in its delay slot.\n"
            "# Equality additionally clears the generated slot; inequality leaves the newly stored halfword intact.\n"
            "# KMC emits an equivalent early store followed by `beql`, which changes scheduling and one instruction.\n"
            "# Preserve the comparison branch and its unconditional store delay slot.\n"
            ".set noreorder\n"
            "lhu $3,0(%0) # load the original halfword from the current local value slot\n"
            "andi $2,%1,0xFFFF # zero-extend the generated callback value into $v0\n"
            "bne $3,$2,.Lbf9c8_value_stored # keep the generated value when it differs from the original\n"
            "sh $2,8(%0) # store the generated value four halfwords ahead in the branch delay slot\n"
            "sh $0,8(%0) # replace an unchanged generated value with zero to mark it as a duplicate\n"
            ".Lbf9c8_value_stored: # join after storing either the generated value or zero\n"
            "# Restore normal assembler scheduling after the duplicate-value join.\n"
            ".set reorder\n"
            : : "r" (slot_or_item), "r" (arg3) : "$2", "$3", "memory");
        index_or_result += 1;
        slot_or_item += 2;
    } while (index_or_result < 5);

    asm volatile(
        "# Hybrid scope: this zero-byte label is the never-taken first-phase branch target and begins the second phase.\n"
        ".Lbf9c8_second_phase: # branch target and entry point for callback aggregation");
    offset_or_count = 0;
    three_or_total = 0;
    one_or_selected_class = arg1 & 0xFF;
    table_or_sentinel = 0x1FF;
    two_or_value_base = (s32)stack_data.values;
    record_or_function_cursor = (s32)D_801EF288;
    do {
        index_or_result = (*(s32 (**)(s32, s32))record_or_function_cursor)(
            one_or_selected_class, one_or_selected_class);
        asm volatile(
            "# Hybrid scope: this zero-byte barrier preserves the raw callback result in $s1 before the C mask into $s0.\n"
            "# Without the input constraint KMC folds the mask into the $s1 assignment and reverses retail's two moves.\n"
            : : "r" (index_or_result));
        slot_or_item = index_or_result & 0xFFFF;
        asm volatile(
            "# Hybrid scope: this branch-likely skips all nonzero-item work for an empty callback result.\n"
            "# When $s0 is zero, the delay slot increments the completed-function count and control resumes at the outer loop test.\n"
            "# KMC lowers the equivalent C `if` as `bne` plus an additional local jump instead of retail's two-instruction `beql`.\n"
            "# Preserve the branch-likely and its taken-only count increment.\n"
            ".set noreorder\n"
            "beql %0,$0,.Lbf9c8_outer_next # skip mapping and scanning when the callback returned no item\n"
            "addiu %1,%1,1 # count the completed callback in the taken-only delay slot\n"
            "# Restore normal assembler scheduling after the empty-item branch.\n"
            ".set reorder\n"
            : : "r" (slot_or_item), "r" (offset_or_count));

        mapped_item = func_000415fc(slot_or_item) & 0xFFFF;
        matched = 0;
        if (mapped_item != table_or_sentinel) {
            mapped_item *= 4;
            matched = *(u8 *)(mapped_item + 0x80196B02);
            sum_offset = *(u8 *)(mapped_item + 0x80196B03);
            matched = (u8)matched < (u8)sum_offset;
        }
        asm volatile(
            "# Hybrid scope: these eleven instructions scan all eight stack halfwords for the current nonzero item.\n"
            "# $a2 is the scan index, $a0 preserves the item, and $a1 walks the stack array beginning at $s6.\n"
            "# `bnel` increments the index only for a mismatch; equality jumps out while incrementing matched flag $v1.\n"
            "# The mismatch path loops while index is below eight and advances the halfword cursor in the branch delay slot.\n"
            "# KMC's C lowering inverts the equality branch and adds a duplicate join mask, producing one extra instruction.\n"
            "# Preserve both scan branches and their explicit delay slots.\n"
            ".set noreorder\n"
            "move $6,$0 # initialize the eight-entry scan index in $a2 to zero\n"
            "move $4,%2 # copy the nonzero mapped item into the comparison register $a0\n"
            "move $5,%3 # point $a1 at the first halfword in the local value array\n"
            ".Lbf9c8_scan_values: # begin one local halfword comparison\n"
            "lhu $2,0($5) # load the current local halfword into $v0\n"
            "bnel $4,$2,.Lbf9c8_scan_continue # continue scanning only when the item does not match\n"
            "addiu $6,$6,1 # increment the scan index in the mismatch-only delay slot\n"
            "j .Lbf9c8_scan_done # leave the scan immediately after an equality match\n"
            "addiu %0,%0,1 # increment the matched-item flag in the jump delay slot\n"
            ".Lbf9c8_scan_continue: # mismatch path after advancing the scan index\n"
            "slti $2,$6,8 # test whether fewer than eight local halfwords have been examined\n"
            "bne $2,$0,.Lbf9c8_scan_values # loop while the scan index remains below eight\n"
            "addiu $5,$5,2 # advance the halfword cursor in the loop-branch delay slot\n"
            ".Lbf9c8_scan_done: # join after a match or after all eight values were examined\n"
            "# Restore normal assembler scheduling after the eight-value scan.\n"
            ".set reorder\n"
            : "=r" (matched)
            : "0" (matched), "r" (slot_or_item), "r" (two_or_value_base)
            : "$2", "$4", "$5", "$6", "memory");
        offset_or_count += 1;
        sum_offset = matched & 0xFF;
        if (sum_offset == 0) {
            asm volatile(
                "# Hybrid scope: this one instruction masks the raw $s1 callback result into sum-table offset $v0.\n"
                "# KMC substitutes the already-masked $s0 value and emits `move $v0,$s0`; retail redundantly masks $s1 here.\n"
                "andi %0,%1,0xFFFF # zero-extend the raw callback result for the sum-table byte offset\n"
                : "=r" (sum_offset) : "r" (index_or_result));
            sum_offset <<= 5;
            three_or_total += *(u16 *)(sum_offset + 0x8018C414);
        }
        asm volatile(
            "# Hybrid scope: this zero-byte label is the empty-item branch target at the outer loop test.\n"
            ".Lbf9c8_outer_next: # join at the callback-count update for empty and nonempty items");
        record_or_function_cursor += 4;
    } while (offset_or_count < 4);

    return three_or_total;
}
