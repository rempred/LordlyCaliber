typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef s32 M2C_UNK;
typedef s8 M2C_UNK8;
typedef s16 M2C_UNK16;
typedef s32 M2C_UNK32;
typedef s64 M2C_UNK64;
s16 func_000bb47c(M2C_UNK, s32, s32);                           
s32 func_000bbd50(u8);                                          
M2C_UNK func_000bf248();                                        
M2C_UNK func_000bf458();                                        
M2C_UNK func_000c6e38(u8);                                      
M2C_UNK func_000cf62c(s32, u8, u8, u8);                         
extern u8 * volatile D_80196AF8;

asm(
    "# func_000d0150 rebuilds class-change screen state for the current actor.\n"
    "# An invalid current-class byte resets result-list and UI state.\n"
    "# The normal path resolves the primary/fallback class, invokes the list helpers,\n"
    "# selects the matching result row, builds four entries, and records acceptance state.\n"
    "# Offset accesses remain until the surrounding structures have reviewed field definitions.\n");
void func_000d0150(s32 arg0) {
    s16 temp_v0_118;
    s32 temp_v0_130;
    s32 temp_v0_53;
    s32 temp_v1_122;
    register s32 var_s1 asm("$17");
    u32 temp_a1_16;
    u8 temp_a1_62;
    u8 temp_a2_59;
    register u32 temp_s3_15 asm("$19");
    u8 temp_v1_56;
    u8 var_a3_48;
    u8 var_s2_66;
    void *temp_a0_121;
    void *temp_a2_8;
    void *temp_v0_41;
    void *temp_v0_78;
    void *temp_v1_25;
    void *var_a0_28;
    volatile u64 frame_pad;

    temp_a2_8 = D_80196AF8;
    temp_s3_15 = (*(u8 *)((s8 *)(temp_a2_8) + (0x18B)));
    temp_a1_16 = temp_s3_15 & 0xFF;
    var_s1 = 3;
    if (((temp_a1_16 == 0) | (temp_a1_16 >= 0x64U)) == 0) {
        goto block_4;
    }
    (*(u8 *)((s8 *)(temp_a2_8) + (0x5EC))) = 0U;
    temp_v1_25 = D_80196AF8;
    (*(s16 *)((s8 *)(temp_a2_8) + (0x5E8))) = 0;
    var_a0_28 = temp_v1_25 + 6;
    (*(s16 *)((s8 *)(temp_v1_25) + (0x5EA))) = 0;
    (*(s16 *)((s8 *)(temp_v1_25) + (0x1C0E))) = 0;
    (*(u16 *)((s8 *)(temp_v1_25) + (0x1BFC))) = 1U;
loop_2:
    (*(s16 *)((s8 *)(var_a0_28) + (0x1BFE))) = 0;
    var_s1 -= 1;
    var_a0_28 -= 2;
    if (var_s1 >= 0) {
        goto loop_2;
    }
    (*(s8 *)((s8 *)D_80196AF8 + (0x60C))) = 0;
    temp_v0_41 = D_80196AF8;
    (*(s16 *)((s8 *)(temp_v0_41) + (0x1BFA))) = 0;
    (*(s16 *)((s8 *)(temp_v0_41) + (0x1BF6))) = 0;
    return;
block_4:
    var_a3_48 = 0;
    if (!(arg0 & 0xFF)) {
        goto block_6;
    }
    var_a3_48 = (*(u8 *)((s8 *)(temp_a2_8) + (0x5E9)));
block_6:
    temp_v0_53 = temp_a1_16 * 0x38;
    temp_v1_56 = (*(u8 *)((s8 *)((temp_v0_53 + 0x80190000)) + (0x3BD1)));
    temp_a2_59 = (*(u8 *)((s8 *)((temp_v1_56 + 0x801F0000)) + (-0xC48)));
    temp_a1_62 = (*(u8 *)((s8 *)((temp_v0_53 + 0x80190000)) + (0x3BD2)));
    if (temp_a2_59 != 0) {
        goto block_8;
    }
    var_s2_66 = temp_a1_62;
    goto block_10;
block_8:
    if ((*(u8 *)((s8 *)(((temp_v1_56 * 0x48) + 0x80180000)) + (0x7C59))) == temp_a1_62) {
        var_s2_66 = temp_a2_59;
    } else {
        var_s2_66 = temp_a1_62;
    }
block_10:
    temp_v0_78 = D_80196AF8;
    (*(u8 *)((s8 *)(temp_v0_78) + (0x5EC))) = var_s2_66;
    (*(s16 *)((s8 *)(temp_v0_78) + (0x5E8))) = (s16) (var_s2_66 & 0xFF);
    if (!(arg0 & 0xFF)) {
        goto block_12;
    }
    var_s2_66 = var_a3_48;
block_12:
    func_000cf62c(var_s2_66 & 0xFF, temp_a1_62, temp_a2_59, var_a3_48);
    func_000c6e38(temp_s3_15);
    var_s1 = 0;
    func_000bf248();
    func_000bf458();
    {
        register void *scan_base asm("$2");
        register u32 scan_v1 asm("$3");
        register s32 scan_count asm("$4");
        register void *scan_data asm("$5");
        register u32 scan_selected asm("$6");

        scan_base = D_80196AF8;
        scan_v1 = (*(u16 *)((s8 *)(scan_base) + (0x1BFC)));
        (*(s16 *)((s8 *)(scan_base) + (0x5EA))) = 0;
        if ((s32)scan_v1 <= 0) {
            goto scan_done;
        }
        asm volatile(
            "# Hybrid scope: this one instruction remasks the selected class into retail's $a2.\n"
            "# The four preceding helpers make the retail compiler discard its earlier byte-range fact.\n"
            "# KMC otherwise proves the u8 C value is already masked and delays the equivalent mask into $v1.\n"
            "andi %0,%1,0x00ff # zero-extend the selected class byte into retail's $a2 scan value\n"
            : "=r" (scan_selected)
            : "r" (var_s2_66));
        scan_count = scan_v1;
        scan_data = scan_base;
        scan_v1 = (u32)scan_data;
scan_loop:
        if (scan_selected == ((*(u16 *)(scan_v1 + 0x1C0E)) & 0x7F)) {
            goto scan_match;
        }
        var_s1 += 1;
        if (var_s1 < scan_count) {
            scan_v1 += 2;
            goto scan_loop;
        }
        goto scan_done;
scan_match:
        (*(s16 *)((s8 *)(scan_data) + (0x5EA))) = var_s1;
scan_done:
        ;
    }
    var_s1 = 0;
    {
        register s32 ui_zero asm("$4");
        register u32 ui_class asm("$5");
        register s32 ui_next asm("$16");
        register s32 ui_test asm("$2");

        asm volatile(
            "# Hybrid scope: these three instructions reproduce retail's class-entry call setup.\n"
            "# move clears $a0 for the helper's first argument on the first iteration.\n"
            "# addiu computes the one-based slot in $s0 from the zero-based $s1 C loop index.\n"
            "# andi masks the original class byte from $s3 into the helper's $a1 argument.\n"
            "# The helper call, returned halfword store, pointer arithmetic, and loop-state update remain C.\n"
            "# Keep the helper-argument setup in retail instruction order.\n"
            ".set noreorder\n"
            "move %0,$0 # clear the helper's first argument for the first list entry\n"
            ".Lfunc_000d0150_ui_loop: # begin one of the four class-entry helper calls\n"
            "addiu %1,%3,1 # compute the one-based entry number from the zero-based loop index\n"
            "andi %2,%5,0x00ff # zero-extend the actor's original class byte for the helper's second argument\n"
            "# Restore assembler instruction reordering after the fixed setup sequence.\n"
            ".set reorder\n"
            : "=r" (ui_zero), "=r" (ui_next), "=r" (ui_class), "=r" (var_s1)
            : "3" (var_s1), "r" (temp_s3_15));
        temp_v0_118 = func_000bb47c(ui_zero, ui_class, ui_next & 0xFF);
        temp_a0_121 = D_80196AF8;
        temp_v1_122 = var_s1 * 2;
        var_s1 = ui_next;
        (*(s16 *)((s8 *)((temp_a0_121 + temp_v1_122)) + (0x1BFE))) = temp_v0_118;
        asm volatile(
            "# Hybrid scope: these three instructions are the retail four-entry loop test and back edge.\n"
            "# slti tests whether the updated $s1 index is still below four.\n"
            "# bnel repeats the setup/call/store sequence only while that C condition is true.\n"
            "# Its taken-only delay slot clears $a0 for the next helper call; on exit $a0 keeps the data pointer.\n"
            "# Preserve the branch-likely loop test and its taken-only delay slot.\n"
            ".set noreorder\n"
            "slti %0,%1,4 # test whether fewer than four class entries have been generated\n"
            "bnel %0,$0,.Lfunc_000d0150_ui_loop # repeat the helper sequence while the updated index is below four\n"
            "move $4,$0 # clear the next helper's first argument only when the likely branch is taken\n"
            "# Restore assembler instruction reordering after the loop back edge.\n"
            ".set reorder\n"
            : "=r" (ui_test)
            : "r" (var_s1));
    }
    temp_v0_130 = func_000bbd50((*(u8 *)((s8 *)(temp_a0_121) + (0x5E9))));
    (*(s8 *)((s8 *)D_80196AF8 + (0x60C))) = (s8) (((u32) ((temp_v0_130 - 1) & 0xFFFF) < 2U) | ((temp_v0_130 & 0xFFFF) == 5));
    (*(s16 *)((s8 *)D_80196AF8 + (0x5E8))) = (s16) (var_s2_66 & 0xFF);
    return;
}

asm(
    "# Hybrid tail ownership: the accepted func_000d0150 range extends five words past its C epilogue.\n"
    "# The first three words are the retail 12-byte alignment gap, so .space emits three zero/NOP words.\n"
    ".space 12 # emit the accepted owner's three zero alignment words\n"
    "# The final two words are the accepted owner's share of the next routine's split preamble.\n"
    "# lui/lw load D_80196AF8 into $a2; they do not execute as part of func_000d0150's gameplay body.\n"
    "lui $6,%hi(D_80196AF8) # load the shared-state pointer address high half into $a2\n"
    "lw $6,%lo(D_80196AF8)($6) # dereference the shared-state pointer into $a2\n"
    ".size func_000d0150,568 # extend ELF ownership through the alignment gap and split preamble\n");
