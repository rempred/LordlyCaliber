# Reuses space released by the complete source-built Goth calculation.
# Always reload at settlement; no stale module can survive a changed-ROM state.
addiu $sp,$sp,-32
sw $ra,28($sp)
sw $a0,20($sp)
sw $a1,24($sp)
lui $a0,0x804c
jal RESERVE_INVALIDATE_I
addiu $a1,$zero,0x1000
lui $a0,0x804c
jal RESERVE_INVALIDATE_D
addiu $a1,$zero,0x1000
lui $t0,0xa460
.Lwaitbefore:
lw $t1,16($t0)
andi $t1,$t1,3
bnez $t1,.Lwaitbefore
nop
lui $t1,0x004c
sw $t1,0($t0)
lui $t1,0x127c
ori $t1,$t1,0x6000
sw $t1,4($t0)
addiu $t1,$zero,0x0fff
sw $t1,12($t0)
.Lwaitafter:
lw $t1,16($t0)
andi $t1,$t1,3
bnez $t1,.Lwaitafter
nop
lw $ra,28($sp)
lw $a0,20($sp)
lw $a1,24($sp)
addiu $sp,$sp,32
j RESERVE_ENTRY
nop
