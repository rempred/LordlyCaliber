# Parameter normalization, z64 [2076A8,20772C), live 801C4218.
# Preserve source/owner pointers and write only the normalized equipment value.
equipment_register:
    lw $v0, 0($s6)
    lw $a0, 0($s5)
    sltiu $v1, $v0, 256
    beq $v1, $zero, register_equipped
    sll $v0, $v0, 1
    lui $v1, 0x801d
    lw $v1, 0x690($v1)
    addu $v0, $v0, $v1
    lhu $v0, -LC_POLICY_BACK($v0)
    andi $v0, $v0, 511
    addiu $v1, $zero, 511
    bne $v0, $v1, register_store
    nop
register_equipped:
    addu $v0, $a0, $zero
register_store:
    sw $v0, 0($s5)
    b register_done
    nop
# Shared policy query occupies this owner's former switch padding. It preserves
# all inputs and caller temporaries apart from v0/v1, including t0 in delay slots.
lc_visual_policy_entry:
    .if lc_visual_policy_entry-equipment_register-68
    .err
    .endif
    sltiu $v0, $a0, 256
    beq $v0, $zero, policy_default
    sll $v0, $a0, 1
    lui $v1, 0x801d
    lw $v1, 0x690($v1)
    addu $v0, $v0, $v1
    lhu $v0, -LC_POLICY_BACK($v0)
    jr $ra
    nop
policy_default:
    jr $ra
    addiu $v0, $zero, 511
register_done:
    .space 132-(.-equipment_register)
