# Body allocation, z64 [205CBC,205D38), live 801C282C.
# The original switch interleaves these three callee-save stores.
equipment_allocate:
    sw $ra, 64($sp)
    sw $s4, 48($sp)
    sw $s1, 36($sp)
    sltiu $v0, $s2, 256
    beq $v0, $zero, allocate_done
    sll $v0, $s2, 1
    lui $v1, 0x801d
    lw $v1, 0x690($v1)
    addu $v0, $v0, $v1
    lhu $v1, -LC_POLICY_BACK($v0)
    andi $v1, $v1, 511
    addiu $v0, $zero, 511
    beq $v1, $v0, allocate_done
    nop
    addu $s0, $v1, $zero
allocate_done:
    .space 124-(.-equipment_allocate)
