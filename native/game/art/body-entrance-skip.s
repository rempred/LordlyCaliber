# z64 [2AADC8,2AADF0), live 8023B668. Secondary linked bodies bypass this block.
    lw $a0, 72($s3)
    jal lc_visual_policy
    nop
    andi $v0, $v0, 0x1800
    beq $v0, $zero, entrance_regular
    sw $s2, 16($sp)
    j lc_entrance_next
    sb $s7, 0($s4)
    nop
    nop
entrance_regular:
