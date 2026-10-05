# Maintained alignment source adapted from parent tools/luck_alignment_display_patch.py.
# Module version 10; assembled during user export.
# Preserve hook arguments; verify sentinel and version before cache-coherent PI load.
    addu $t4,$ra,$zero
    addu $t5,$a0,$zero
    addu $t6,$a1,$zero
    lui $t0,0xa04a
    ori $t0,$t0,0x0
    lw $t1,0x0($t0)
    lui $t2,0x4f42
    ori $t2,$t2,0x414c
    bne $t1,$t2,bootstrap_reload
    nop
    lw $t1,0x4($t0)
    addiu $t2,$zero,0xa
    beq $t1,$t2,bootstrap_loaded
    nop
bootstrap_reload:
    lui $a0,0x804a
    ori $a0,$a0,0x0
    lui $a1,0x0
    ori $a1,$a1,0xd00
    jal TOOL_INVALIDATE_ICACHE
    nop
    jal TOOL_INVALIDATE_DCACHE
    nop
    lui $t0,0xa460
    ori $t0,$t0,0x0
bootstrap_wait_before:
    lw $t1,0x10($t0)
    andi $t1,$t1,0x3
    bne $t1,$zero,bootstrap_wait_before
    nop
    lui $t1,0x4a
    ori $t1,$t1,0x0
    sw $t1,0x0($t0)
    lui $t1,0x127c
    ori $t1,$t1,0x3000
    sw $t1,0x4($t0)
    lui $t1,0x0
    ori $t1,$t1,0xcff
    sw $t1,0xc($t0)
bootstrap_wait_after:
    lw $t1,0x10($t0)
    andi $t1,$t1,0x3
    bne $t1,$zero,bootstrap_wait_after
    nop
bootstrap_loaded:
    addu $ra,$t4,$zero
    addu $a0,$t5,$zero
    addu $a1,$t6,$zero
    j TOOL_ENTRY
    nop
.org 0xc0
