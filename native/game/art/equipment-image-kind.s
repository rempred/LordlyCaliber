# z64 [2064EC,206510): conditional kind-two to equipment-child selection.
    lw $t0, 60($sp)
    nop
    nop
    addu $v0, $t0, $zero
    beq $v0, $zero, kind_done
    andi $v1, $a0, 255
    addiu $v0, $zero, 2
    beql $v1, $v0, kind_done
    addiu $a0, $zero, 1
kind_done:
