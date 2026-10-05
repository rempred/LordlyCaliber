# z64 [2AB884,2AB8BC), live 8023C124. Preserve owner-one/linked-owner priority.
# s0=1, s6=source, s2=previous Danika body; result s5 feeds the original constructor.
    beq $v0, $s0, construction_pair
    addu $a0, $s6, $zero
    jal lc_visual_policy
    nop
    andi $v0, $v0, 0x1800
    srl $v0, $v0, 11
    bne $v0, $s0, construction_done
    addu $s5, $v0, $zero
    xor $s5, $s2, $s0
    sltiu $s5, $s5, 1
    b construction_done
    sll $s5, $s5, 2
construction_pair:
    addiu $s5, $zero, 2
construction_done:
    addu $s3, $zero, $zero
