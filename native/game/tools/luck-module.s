# Maintained luck source adapted from parent tools/luck_alignment_display_patch.py.
# Module version 6; assembled during user export.
    .word 0x4f424c4b,0x00000006,0x00000c00,0x00000100
    .word 0x00000500,0x00000520,0x00000720,0x000000f8
    .word 0x00e70029
.org 0x100
feature_entry:
    addiu $sp,$sp,-32
    sw $ra,0x0($sp)
    sw $a0,0x4($sp)
    sw $a1,0x8($sp)
    lbu $v1,0x28($s5)
    lui $a3,0x8048
    ori $a3,$a3,0x720
    jal display_draw
    nop
luck_after_custom_draw:
    lw $a0,0x4($sp)
    lw $a1,0x8($sp)
    jal TOOL_NATIVE_DRAW
    nop
luck_after_native_draw:
    lw $ra,0x0($sp)
    addiu $sp,$sp,0x20
    jr $ra
    nop
display_draw:
    sltiu $t9,$v1,0x65
    bne $t9,$zero,display_value_clamped
    nop
    addiu $v1,$zero,0x64
display_value_clamped:
    lui $t0,0x800e
    ori $t0,$t0,0x9ba0
    lw $a2,0x0($t0)
    beq $a2,$zero,display_draw_done
    nop
    addu $t0,$a3,$zero
    addu $t1,$a2,$zero
    addiu $t2,$zero,0x3e
display_copy_loop:
    lw $t3,0x0($t0)
    sw $t3,0x0($t1)
    addiu $t0,$t0,0x4
    addiu $t1,$t1,0x4
    addiu $t2,$t2,-1
    bne $t2,$zero,display_copy_loop
    nop
    lui $t0,0x800e
    ori $t0,$t0,0x9ba0
    addiu $t1,$a2,0xf8
    sw $t1,0x0($t0)
    sltiu $t4,$v1,0x64
    sltiu $t5,$v1,0xa
    addu $t6,$v1,$zero
    addiu $t7,$zero,0x0
    sltiu $t9,$t6,0x64
    bne $t9,$zero,display_under_100
    nop
    addiu $t6,$t6,-100
    addiu $t7,$zero,0x1
display_under_100:
    addiu $t8,$zero,0x0
display_tens_loop:
    sltiu $t9,$t6,0xa
    bne $t9,$zero,display_digits_ready
    nop
    addiu $t6,$t6,-10
    addiu $t8,$t8,0x1
    j display_tens_loop
    nop
display_digits_ready:
    bne $t4,$zero,display_left_under_100
    nop
    sll $t0,$t7,0x1
    addu $t0,$t0,$t7
    sll $t0,$t0,0x6
    sll $t0,$t0,0x10
    ori $t0,$t0,0x100
    sw $t0,0xa4($a2)
    sll $t0,$t8,0x1
    addu $t0,$t0,$t8
    sll $t0,$t0,0x6
    sll $t0,$t0,0x10
    ori $t0,$t0,0x100
    sw $t0,0xc4($a2)
    sll $t0,$t6,0x1
    addu $t0,$t0,$t6
    sll $t0,$t0,0x6
    sll $t0,$t0,0x10
    ori $t0,$t0,0x100
    sw $t0,0xe4($a2)
    j display_draw_done
    nop
display_left_under_100:
    bne $t5,$zero,display_left_under_10
    nop
    sll $t0,$t8,0x1
    addu $t0,$t0,$t8
    sll $t0,$t0,0x6
    sll $t0,$t0,0x10
    ori $t0,$t0,0x100
    sw $t0,0xa4($a2)
    sll $t0,$t6,0x1
    addu $t0,$t0,$t6
    sll $t0,$t0,0x6
    sll $t0,$t0,0x10
    ori $t0,$t0,0x100
    sw $t0,0xc4($a2)
    j display_draw_done
    nop
display_left_under_10:
    sll $t0,$t6,0x1
    addu $t0,$t0,$t6
    sll $t0,$t0,0x6
    sll $t0,$t0,0x10
    ori $t0,$t0,0x100
    sw $t0,0xa4($a2)
display_draw_done:
    jr $ra
    nop
.org 0x500
# Transparent, outline, cream and shadow TLUT entries.
    .word 0x00002907,0xf7b594d3,0x00000000,0x00000000
    .word 0x00000000,0x00000000,0x00000000,0x00000000
.org 0x520
# The export compiler appends the current-ROM-derived CI4 texture here.
