# Maintained source adapted from parent tools/build_cf_army_counter_modregion_rom.py
# and Decomp e50fe500 tools/lordly_phase3_native_payload.py. Assembled at user export.
# Invalidate caches, wait for PI DMA, then enter the loaded module.
    # The loader is called at the task writeback boundary. Cache service
    # arguments are not the task arguments; preserve both across the cold load.
    addiu $sp,$sp,-32
    sw $ra,28($sp)
    sw $a0,16($sp)
    sw $a1,20($sp)
    lui $a0,0x8042
    ori $a0,$a0,0x0
    lui $a1,0x0
    ori $a1,$a1,0xa00
    jal CF_INVALIDATE_ICACHE
    nop
    lui $a0,0x8042
    ori $a0,$a0,0x0
    addiu $a1,$zero,0xa00
    jal CF_INVALIDATE_DCACHE
    nop
    lw $ra,28($sp)
    lw $a0,16($sp)
    lw $a1,20($sp)
    addiu $sp,$sp,32
    lui $t0,0xa460
    ori $t0,$t0,0x0
wait_pi_before:
    lw $t1,0x10($t0)
    andi $t1,$t1,0x3
    bne $t1,$zero,wait_pi_before
    nop
    lui $t1,0x42
    ori $t1,$t1,0x0
    sw $t1,0x0($t0)
    lui $t1,0x127c
    ori $t1,$t1,0x1000
    sw $t1,0x4($t0)
    lui $t1,0x0
    ori $t1,$t1,0x9ff
    sw $t1,0xc($t0)
wait_pi_after:
    lw $t1,0x10($t0)
    andi $t1,$t1,0x3
    bne $t1,$zero,wait_pi_after
    nop
    j CF_ENTRY
    nop
.org 0xc0
