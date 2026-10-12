# Expansion Pak module at RAM804C0000; ROM027C6000..027C7000.
# Invoked once by the normal mission-settlement item award. Key54 skips that
# caller. Source rows are the scenario's 30 persistent player-squad records.
.ascii "LCRESXP1"
.word RESERVE_XP
.word 0
reserve_entry:
addiu $sp,$sp,-160
sw $ra,156($sp)
sw $s0,152($sp)
sw $s1,148($sp)
sw $s2,144($sp)
sw $s3,140($sp)
jal RESERVE_ITEM
nop
sw $v0,136($sp)
addiu $t0,$sp,32
addiu $t1,$sp,132
.Lclear:
sb $zero,0($t0)
addiu $t0,$t0,1
bne $t0,$t1,.Lclear
nop
lui $t0,0x8019
addiu $t0,$t0,0x71f0
addiu $t1,$zero,30
.Lunit:
lbu $t2,1($t0)
andi $t2,$t2,1
beqz $t2,.Lnextunit
addiu $t3,$zero,0
.Lmember:
addu $t4,$t0,$t3
lbu $t2,2($t4)
sltiu $t4,$t2,100
beqz $t4,.Lnextmember
addu $t4,$sp,$t2
addiu $t5,$zero,1
sb $t5,32($t4)
.Lnextmember:
addiu $t3,$t3,1
sltiu $t4,$t3,5
bnez $t4,.Lmember
nop
.Lnextunit:
addiu $t1,$t1,-1
bnez $t1,.Lunit
addiu $t0,$t0,25
lui $s0,0x8019
addiu $s0,$s0,0x3bf8
addiu $s1,$sp,33
addiu $s2,$sp,132
lui $t0,0x804c
lw $s3,8($t0)
.Lcharacter:
lbu $t0,0($s1)
bnez $t0,.Lnextchar
lbu $t0,17($s0)
sltiu $t0,$t0,2
bnez $t0,.Lnextchar
lbu $t0,19($s0)
sltiu $t0,$t0,99
beqz $t0,.Lnextchar
lhu $t0,24($s0)
beqz $t0,.Lnextchar
lbu $t0,51($s0)
andi $t0,$t0,6
bnez $t0,.Lnextchar
lbu $t0,53($s0)
addu $t0,$t0,$s3
sltiu $t1,$t0,100
bnez $t1,.Lstorexp
nop
jal RESERVE_GROW
addu $a0,$s0,$zero
addu $t0,$zero,$zero
.Lstorexp:
sb $t0,53($s0)
.Lnextchar:
addiu $s1,$s1,1
bne $s1,$s2,.Lcharacter
addiu $s0,$s0,56
lw $v0,136($sp)
lw $s3,140($sp)
lw $s2,144($sp)
lw $s1,148($sp)
lw $s0,152($sp)
lw $ra,156($sp)
jr $ra
addiu $sp,$sp,160
.org 0x1000
