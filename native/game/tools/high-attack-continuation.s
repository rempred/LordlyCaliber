# Maintained US Rev0 source adapted from parent tools/build_high_attack_stream_shift_rom.py.
# Version 23 repairs loader identity and reconciles known references at each active insert.
# Experimental; Project64 Interpreter only. No per-site cache experiment.
    addiu $sp,$sp,-24
    sw $ra,20($sp)
    sw $a0,16($sp)
    sw $a1,12($sp)
    lui $a0,32836
    ori $a1,$zero,6736
    jal HA_800900C0
    nop
    jal HA_80090010
    nop
    lw $ra,20($sp)
    lw $a0,16($sp)
    lw $a1,12($sp)
    addiu $sp,$sp,24
    lui $t0,42080
continuation_wait_before:
    lw $t1,16($t0)
    andi $t1,$t1,3
    bne $t1,$zero,continuation_wait_before
    nop
    lui $t1,68
    sw $t1,0($t0)
    lui $t1,4730
    sw $t1,4($t0)
    ori $t1,$zero,6735
    sw $t1,12($t0)
continuation_wait_after:
    lw $t1,16($t0)
    andi $t1,$t1,3
    bne $t1,$zero,continuation_wait_after
    nop
    lui $t0,32836
    ori $t0,$t0,64
    jr $t0
    nop
.org 0xc0
