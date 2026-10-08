.set noreorder
.set noat
/* a0: pressed-button mask; s0: category. Empty categories cannot be bought.
 * This helper fits in the replaced producer's unused tail, at ROM 19BF60.
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


