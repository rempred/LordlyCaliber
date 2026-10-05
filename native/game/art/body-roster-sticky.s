# z64 [2ABD68,2ABD80), live 8023C608. Mark the shared Danika-body construction flag.
    lw $a0, 72($s2)
    jal lc_visual_policy
    addiu $t0, $zero, 1
    andi $v0, $v0, 0x800
    bnel $v0, $zero, roster_sticky_done
    sw $t0, 76($sp)
roster_sticky_done:
