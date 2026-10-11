# Maintained US Rev0 source adapted from parent tools/build_high_attack_stream_shift_rom.py.
# Version 23 repairs loader identity and reconciles known references at each active insert.
# Experimental; Project64 Interpreter only. No per-site cache experiment.
.if HA_SITE_0
.if HA_RETAIL
lui $v0,0x801d
lw $v0,-5952($v0)
.else
jal HA_800A4B00
nop
.endif
.endif
.if HA_SITE_1
.if HA_RETAIL
beq $s4,$zero,.+0x40
.else
beq $s4,$zero,.+0x1ff0
.endif
.endif
.if HA_SITE_2
.if HA_RETAIL
jal HA_801AD974
addiu $a0,$a0,68
.else
jal HA_80440620
addiu $a0,$a0,68
.endif
.endif
.if HA_SITE_3
.if HA_RETAIL
lw $a0,72($v1)
lw $a1,76($v1)
.else
jal HA_80440680
lw $a1,76($v1)
.endif
.endif
.if HA_SITE_4
.if HA_RETAIL
addiu $t0,$zero,1
lui $at,0x801d
.else
j HA_80440700
nop
.endif
.endif
.if HA_SITE_5
.if HA_RETAIL
addu $s0,$v0,$zero
addiu $s1,$s0,210
.else
jal HA_804407D0
nop
.endif
.endif
.if HA_SITE_6
.if HA_RETAIL
jal HA_800712C4
lw $a0,-5956($a0)
.else
jal HA_80440820
lw $a0,-5956($a0)
.endif
.endif
.if HA_SITE_7
.if HA_RETAIL
j HA_801D28FC
addiu $s2,$zero,-1
.else
j HA_80440900
nop
.endif
.endif
