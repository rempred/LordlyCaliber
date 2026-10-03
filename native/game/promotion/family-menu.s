# Modified-game complete func_000BF248, same 528-byte extent and calling ABI.
# Families are full u16 IDs. Behavior is inherited from a retail family,
# preserving disabled families and the two human-family special targets.
# Class IDs/count and the existing discovered-bit/list storage are unchanged.
.extern FAMILY_LOOKUP
.extern CHECK_CHANGE
addiu $sp, $sp, -48
sw $ra, 44($sp)
sw $s6, 40($sp)
sw $s5, 36($sp)
sw $s4, 32($sp)
sw $s3, 28($sp)
sw $s2, 24($sp)
sw $s1, 20($sp)
sw $s0, 16($sp)
lui $s4, 0x8019
lw $s4, 0x6AF8($s4)
lbu $a0, 0x5E9($s4)
jal FAMILY_LOOKUP
move $s3, $zero
andi $s0, $v0, 65535
move $s1, $v1
bne $s0, $zero, family_scan
nop
lhu $v0, 0x5E8($s4)
ori $v0, $v0, 128
sh $v0, 0x1C0E($s4)
b family_done
addiu $s3, $zero, 1
family_scan:
lw $s5, 0x80($s4)
addiu $s5, $s5, 648
move $s2, $zero
family_next:
lw $v0, 0($s5)
andi $v0, $v0, 65535
beq $v0, $s0, family_discovered
addiu $t0, $zero, 1
beq $s1, $t0, family_human_one
addiu $t0, $zero, 2
bne $s1, $t0, family_advance
addiu $t0, $zero, 0x26
beq $s2, $t0, family_discovered
nop
b family_advance
nop
family_human_one:
addiu $t0, $zero, 0x26
beq $s2, $t0, family_discovered
addiu $t0, $zero, 0x2B
bne $s2, $t0, family_advance
nop
family_discovered:
srl $t0, $s2, 3
lui $t1, 0x8019
addu $t1, $t1, $t0
lbu $t1, 0x6A76($t1)
andi $t0, $s2, 7
lui $t2, 0x801F
addu $t2, $t2, $t0
lbu $t2, -0xC90($t2)
and $t1, $t1, $t2
beq $t1, $zero, family_advance
nop
move $s6, $s2
lhu $t0, 0x5E8($s4)
beq $t0, $s2, family_disabled
addiu $t0, $s1, -1
sltiu $t0, $t0, 2
bne $t0, $zero, family_check
addiu $t0, $zero, 5
bne $s1, $t0, family_disabled
nop
family_check:
lbu $a0, 0x18B($s4)
jal CHECK_CHANGE
move $a1, $s2
andi $v0, $v0, 255
bne $v0, $zero, family_store
nop
family_disabled:
ori $s6, $s6, 128
family_store:
sll $t0, $s3, 1
addu $t0, $s4, $t0
sh $s6, 0x1C0E($t0)
addiu $s3, $s3, 1
family_advance:
addiu $s2, $s2, 1
sltiu $v0, $s2, 81
bne $v0, $zero, family_next
addiu $s5, $s5, 4
family_done:
sh $s3, 0x1BFC($s4)
sll $t0, $s3, 1
addu $t0, $s4, $t0
sh $zero, 0x1C0E($t0)
lw $ra, 44($sp)
lw $s6, 40($sp)
lw $s5, 36($sp)
lw $s4, 32($sp)
lw $s3, 28($sp)
lw $s2, 24($sp)
lw $s1, 20($sp)
lw $s0, 16($sp)
jr $ra
addiu $sp, $sp, 48
.org 528
