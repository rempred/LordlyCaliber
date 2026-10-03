.set noat
.set noreorder
/* Replace only validation/conversion/SRAM transfer. The surrounding game's
 * Controller Pak detection, read, error handling and summary refresh remain.
 * File index is s6; file allocation and summary header are s5/s8.
 */
lui $v0, 0x8019
lw $v0, 0x780C($v0)
lw $a0, 0x48($v0)
sll $a2, $s6, 5
addu $a2, $v0, $a2
lw $a2, 0x54($a2)
jal lc_pak_import
move $a1, $s5
beq $v0, $zero, rejected
nop
j lc_pak_import_continue
nop
rejected:
j lc_pak_import_reject
nop
