.set noat
.set noreorder
/* The original owner just allocated 0x1900 bytes into v0. Build the file with
 * normalized SRAM data and zero-seeded Pak checksums before device acquisition.
 */
move $s4, $v0
lui $v0, 0x8019
lw $v0, 0x780C($v0)
lw $a0, 0x48($v0)
move $a1, $s4
jal lc_pak_export
addiu $a2, $zero, 0x1900
beq $v0, $zero, rejected
nop
j lc_pak_export_continue
nop
rejected:
j lc_pak_export_reject
move $s5, $zero
