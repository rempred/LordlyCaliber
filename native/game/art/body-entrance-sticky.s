# z64 [2AAFA4,2AAFBC), live 8023B844. s6/s7 hold coordinates, not constants here.
    lw $a0, 72($s3)
    jal lc_visual_policy
    addiu $t0, $zero, 1
    andi $v0, $v0, 0x800
    bnel $v0, $zero, entrance_sticky_done
    sw $t0, 68($sp)
entrance_sticky_done:
