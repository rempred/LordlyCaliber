# Settlement population bonus, z64 1399c0..139ab4 / RAM801e5280.
# a0=Chaos Frame, a1=captured population, a2=base Goth. The following native
# code applies wallet cap, key54 exclusion, item and Soldier transfers.
slti $v0,$a0,50
beqz $v0,.Lhigh
addiu $v1,$zero,50
subu $v0,$v1,$a0
mult $v0,$a1
mflo $v0
addiu $v1,$zero,LOW_DIVISOR
b .Ldivide
nop
.Lhigh:
addiu $v0,$a0,-50
mult $v0,$a1
mflo $v0
addiu $v1,$zero,HIGH_DIVISOR
.Ldivide:
.word 0x0043001a # div zero,v0,v1; validated positive divisor
mflo $v0
addu $v0,$v0,$a2
lui $at,0x8022
sw $v0,-14524($at)
b .Lend
nop
.org 0xf4
.Lend:
