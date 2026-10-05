# Image selection, z64 [206370,206400), live 801C2EE0.
# Keep all original saved registers/arguments. The intervening labels after this
# allocation are live cache-return paths; jump over them to the original join.
equipment_image:
    sw $ra, 100($sp)
    sw $s7, 92($sp)
    sw $s2, 72($sp)
    sw $s1, 68($sp)
    sw $s0, 64($sp)
    sw $a1, 28($sp)
    sw $a3, 36($sp)
    sw $zero, 60($sp)
    lw $a0, 124($sp)
    sltiu $v0, $s8, 256
    beq $v0, $zero, image_done
    sll $v0, $s8, 1
    lui $v1, 0x801d
    lw $v1, 0x690($v1)
    addu $v0, $v0, $v1
    lhu $v1, -LC_POLICY_BACK($v0)
    andi $v0, $v1, 512
    sw $v0, 60($sp)
    andi $v1, $v1, 511
    addiu $v0, $zero, 511
    beq $v1, $v0, image_done
    nop
    addu $a0, $v1, $zero
image_done:
    j lc_image_join
    nop
    .space 144-(.-equipment_image)
