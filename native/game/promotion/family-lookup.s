# Modified-game replacement for the complete 48-byte func_000BBD50 owner.
# Every known caller runs after a promotion-resource load into owner+0x80.
# v0 returns behavior:family packed as u16:u16; old family consumers mask u16.
# v1 returns behavior separately for the two eligibility predicates.
.ifndef CLASS_COUNT
CLASS_COUNT = 81
.endif
.ifndef FAMILY_OFFSET
FAMILY_OFFSET = 648
.endif
andi $a0, $a0, 255
sltiu $v0, $a0, CLASS_COUNT
beq $v0, $zero, family_return
sll $a0, $a0, 2
lui $v1, 0x8019
lw $v1, 0x6AF8($v1)
lw $v1, 0x80($v1)
addu $v1, $v1, $a0
lw $v0, FAMILY_OFFSET($v1)
family_return:
jr $ra
srl $v1, $v0, 16
.org 48
