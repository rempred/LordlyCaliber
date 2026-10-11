/* The ordinary bank-5 reader receives class IDs. Shared HELP references call
 * the lower resource reader directly and keep indices200..222. The Magnus
 * Prince case supplies256, a value outside all byte-sized class IDs.
 * Replay the two displaced prologue instructions, then resume its owner.
 */
.set noreorder
.set noat
    addiu $sp, $sp, -0x88
    sw $s6, 0x80($sp)
    addiu $t0, $zero, 5
    bne $a1, $t0, resume
    sltiu $t0, $a2, 165
    bne $t0, $zero, resume
    sltiu $t0, $a2, 256
    bne $t0, $zero, added
    addiu $t0, $zero, 256
    bne $a2, $t0, resume
    nop
    j resume
    addiu $a2, $zero, 224
added:
    addiu $a2, $a2, 60
resume:
    j lc_description_continue
    nop
