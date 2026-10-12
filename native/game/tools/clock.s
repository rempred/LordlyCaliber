# Shared game clock, z64 0x4271c / RAM 0x8016c81c, original 0xe4-byte owner.
# Only the number of internal seconds per displayed minute changes. Existing
# scheduler speed, save fields, hour boundaries and native calendar remain.
addiu $sp,$sp,-24
sw $ra,16($sp)
lui $a0,0x8019
addiu $a0,$a0,0x6a2c
lbu $v0,9($a0)
lbu $v1,8($a0)
addiu $v0,$v0,1
sb $v0,9($a0)
andi $v0,$v0,255
sltu $v0,$v0,$v1
bnez $v0,.Ldone
nop
sb $zero,9($a0)
lhu $v0,0($a0)
addiu $v0,$v0,-6
sltiu $v0,$v0,12
addiu $v1,$zero,NIGHT_SECONDS
beqz $v0,.Lseconds
nop
addiu $v1,$zero,DAY_SECONDS
.Lseconds:
lbu $v0,3($a0)
addiu $v0,$v0,1
sltu $a1,$v0,$v1
bnez $a1,.Ldone
sb $v0,3($a0)
sb $zero,3($a0)
lbu $v0,2($a0)
addiu $v0,$v0,1
sltiu $v1,$v0,60
bnez $v1,.Ldone
sb $v0,2($a0)
sb $zero,2($a0)
lhu $v0,0($a0)
addiu $v0,$v0,1
sltiu $v1,$v0,24
bnez $v1,.Ldone
sh $v0,0($a0)
sh $zero,0($a0)
jal CLOCK_NEXT_DATE
addiu $a0,$a0,4
.Ldone:
lw $ra,16($sp)
jr $ra
addiu $sp,$sp,24
.org 0xdc
.ascii "LCCLOCK1"
