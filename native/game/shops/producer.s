/* Replace only the combined-list producer inside the original shop owner.
 * v1 is the town's shop ID. The original prefix allocated and cleared 512 bytes;
 * the original suffix releases archive resources and restores saved registers.
 * Each authored row is 176 bytes, including its zero terminator and padding.
 */
.set noreorder
.set noat
    sltiu $t0, $v1, 36
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
    .space 100
cleanup:
