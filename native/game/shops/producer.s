/* Replace only the combined-list producer inside the original shop owner.
 * v1 is the town's shop ID. The original prefix allocated and cleared 512 bytes;
 * the original suffix releases archive resources and restores saved registers.
 * Each authored row is 176 bytes, including its zero terminator and padding.
 */
.set noreorder
.set noat
    sltiu $t0, $v1, LC_SHOP_COUNT
    bne $t0, $zero, valid
    nop
    addu $v1, $zero, $zero
valid:
    sll $t0, $v1, 3
    sll $t1, $v1, 1
    addu $t0, $t0, $t1
    addu $t0, $t0, $v1
    sll $t0, $t0, 4
    lui $a0, %hi(lc_shop_rows)
    addiu $a0, $a0, %lo(lc_shop_rows)
    addu $a0, $a0, $t0
    lui $a1, 0x8022
    lw $a1, -0x60e0($a1)
    jal lc_dma_read
    addiu $a2, $zero, 176
    b cleanup
    nop
/* a0: pressed-button mask; s0: category. Empty categories cannot be bought.
 * This helper fits in the replaced producer's unused tail, at byte72.
 */
selection_guard:
    addu $v0, $a0, $zero
    beq $a0, $zero, valid_selection
    sltiu $t0, $s0, 7
    beq $t0, $zero, invalid_selection
    sll $t0, $s0, 2
    addu $t0, $t0, $s0
    sll $t0, $t0, 1
    lui $t1, 0x8022
    lw $t1, -0x60c0($t1)
    addu $t0, $t0, $t1
    lh $t1, 6($t0)
    bltz $t1, invalid_selection
    lh $t2, 0($t0)
    bltz $t2, invalid_selection
    nop
    slt $t0, $t1, $t2
    bne $t0, $zero, invalid_selection
    nop
valid_selection:
    jr $ra
    nop
invalid_selection:
    jr $ra
    addu $v0, $zero, $zero
    .space 12
cleanup:
