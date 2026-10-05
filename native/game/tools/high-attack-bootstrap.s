# Maintained US Rev0 source adapted from parent tools/build_high_attack_stream_shift_rom.py.
# Version 23 repairs loader identity and reconciles known references at each active insert.
# Experimental; Project64 Interpreter only. No per-site cache experiment.
    lui $t0,0xa044
    lw $t1,0($t0)
    lui $t2,0x4f42
    ori $t2,$t2,0x4853
    bne $t1,$t2,bootstrap_reload
    nop
    lw $t1,4($t0)
    ori $t2,$zero,23
    beq $t1,$t2,bootstrap_loaded
    nop
bootstrap_reload:
    j HA_800A020C
    nop
bootstrap_loaded:
    j HA_80440040
    nop
.org 108
