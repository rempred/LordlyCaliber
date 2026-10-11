# Maintained alignment source adapted from parent tools/luck_alignment_display_patch.py.
# Module version 10; assembled during user export.
    .word 0x4f42414c,0x0000000a,0x00000d00,0x00000100
    .word 0x00000500,0x00000520,0x00000720,0x000005d0
    .word 0x00880014
.org 0x100
feature_entry:
    addiu $sp,$sp,-48
    sw $ra,0x0($sp)
    lw $t0,0x40($sp)
    sw $t0,0x10($sp)
    lw $t0,0x44($sp)
    sw $t0,0x14($sp)
    lw $t0,0x48($sp)
    sw $t0,0x18($sp)
alignment_before_native_row_draw:
    jal TOOL_NATIVE_DRAW
    nop
alignment_after_native_row_draw:
    sltiu $t9,$s7,0x6
    beq $t9,$zero,alignment_row_done
    nop
    addiu $t9,$s7,0x1
    lw $t0,0x1d4($sp)
    beq $t0,$zero,alignment_row_done
    nop
    addiu $t1,$zero,0x6
    beq $t9,$t1,alignment_batch_prepare
    nop
    lbu $t1,0xd($t0)
    andi $t2,$t1,0x80
    beq $t2,$zero,alignment_batch_prepare
    nop
    lbu $t2,0xc($t0)
    andi $t2,$t2,0x1
    bne $t2,$zero,alignment_batch_prepare
    nop
    andi $t1,$t1,0x7f
    beq $t1,$zero,alignment_batch_prepare
    nop
    addiu $t1,$t1,-1
    sltiu $t2,$t1,0x28
    bne $t2,$zero,alignment_row_done
    nop
alignment_batch_prepare:
    sll $t1,$s7,0x3
    subu $t0,$t0,$t1
    sw $t0,0x1c($sp)
    sw $zero,0x20($sp)
    sw $t9,0x24($sp)
alignment_batch_loop:
    lw $t0,0x1c($sp)
    lbu $t1,0x5($t0)
    andi $t1,$t1,0x7f
    beq $t1,$zero,alignment_batch_fallback
    nop
    addiu $a0,$t1,-1
    sltiu $t9,$a0,0x28
    beq $t9,$zero,alignment_batch_fallback
    nop
    jal alignment_average
    nop
    j alignment_batch_value_ready
    nop
alignment_batch_fallback:
    addiu $v1,$zero,0x32
alignment_batch_value_ready:
    lw $t1,0x20($sp)
    sll $t0,$t1,0x8
    sll $t2,$t1,0x3
    subu $t0,$t0,$t2
    lui $a3,0x804a
    ori $a3,$a3,0x720
    addu $a3,$a3,$t0
    jal display_draw
    nop
    lw $t0,0x1c($sp)
    addiu $t0,$t0,0x8
    sw $t0,0x1c($sp)
    lw $t1,0x20($sp)
    addiu $t1,$t1,0x1
    sw $t1,0x20($sp)
    lw $t9,0x24($sp)
    sltu $t2,$t1,$t9
    bne $t2,$zero,alignment_batch_loop
    nop
alignment_row_done:
    lw $ra,0x0($sp)
    addiu $sp,$sp,0x30
    jr $ra
    nop
alignment_average:
    sltiu $t9,$a0,0x28
    beq $t9,$zero,alignment_average_fallback
    nop
    sll $t2,$a0,0x4
    sll $t3,$a0,0x3
    addu $t2,$t2,$t3
    addu $t2,$t2,$a0
    lui $t3,0x8019
    ori $t3,$t3,0x71f2
    addu $t2,$t2,$t3
    addiu $t3,$zero,0x5
    addiu $t5,$zero,0x0
    addiu $t6,$zero,0x0
alignment_member_loop:
    lbu $t4,0x0($t2)
    beq $t4,$zero,alignment_next_member
    nop
    sltiu $t9,$t4,0x63
    beq $t9,$zero,alignment_next_member
    nop
    sll $t7,$t4,0x6
    sll $t8,$t4,0x3
    subu $t7,$t7,$t8
    lui $t8,0x8019
    ori $t8,$t8,0x3bc0
    addu $t7,$t7,$t8
    lbu $t7,0x1b($t7)
    addu $t5,$t5,$t7
    addiu $t6,$t6,0x1
alignment_next_member:
    addiu $t2,$t2,0x1
    addiu $t3,$t3,-1
    bne $t3,$zero,alignment_member_loop
    nop
    beq $t6,$zero,alignment_average_fallback
    nop
    addiu $v1,$zero,0x0
alignment_average_divide_loop:
    sltu $t9,$t5,$t6
    bne $t9,$zero,alignment_average_ready
    nop
    subu $t5,$t5,$t6
    addiu $v1,$v1,0x1
    j alignment_average_divide_loop
    nop
alignment_average_fallback:
    addiu $v1,$zero,0x32
alignment_average_ready:
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
    sll $t0,$t6,0x1
    addu $t0,$t0,$t6
    sll $t0,$t0,0x6
    sll $t0,$t0,0x10
    ori $t0,$t0,0x100
    sw $t0,0xe4($a2)
    bne $t5,$zero,display_skip_tens
    nop
    sll $t0,$t8,0x1
    addu $t0,$t0,$t8
    sll $t0,$t0,0x6
    sll $t0,$t0,0x10
    ori $t0,$t0,0x100
    sw $t0,0xc4($a2)
display_skip_tens:
    bne $t4,$zero,display_skip_hundreds
    nop
    sll $t0,$t7,0x1
    addu $t0,$t0,$t7
    sll $t0,$t0,0x6
    sll $t0,$t0,0x10
    ori $t0,$t0,0x100
    sw $t0,0xa4($a2)
display_skip_hundreds:
display_draw_done:
    jr $ra
    nop
.org 0x500
# Transparent, outline, cream and shadow TLUT entries.
    .word 0x00002907,0xf7b594d3,0x00000000,0x00000000
    .word 0x00000000,0x00000000,0x00000000,0x00000000
.org 0x520
# The export compiler appends the current-ROM-derived CI4 texture here.
