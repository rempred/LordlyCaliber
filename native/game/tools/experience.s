# Rev0 combat overlay func_002190AC, live801D5DDC. Three bounded instruction
# regions adapted from parent tools/experience_scale_patch.py. Assemble during
# user export. XP_SETUP/XP_MINIMUM select regions; XP_RETAIL restores100%.
.if XP_SETUP
  .if XP_RETAIL
    lui $at,0x3f80
    mtc1 $at,$f4
    lui $at,0x801e
    ldc1 $f8,0x66d0($at)
    lui $at,0x801e
    ldc1 $f6,0x66d8($at)
  .else
    lui $at,XP_HI
    ori $at,$at,XP_LO
    mtc1 $at,$f4
    lui $at,0x801e
    ldc1 $f8,0x66d0($at)
    ldc1 $f6,0x66d8($at)
  .endif
.else
.if XP_MINIMUM
  .if XP_RETAIL
    mtc1 $v0,$f0
    nop
    cvt.s.w $f0,$f0
    c.lt.s $f4,$f0
    nop
    nop
    bc1f .Lminimum_done
    mov.s $f2,$f4
    mov.s $f2,$f0
.Lminimum_done:
    nop
  .else
    slti $at,$v0,1
    beq $at,$zero,.Lkeep_level
    nop
    addiu $v0,$zero,1
.Lkeep_level:
    mtc1 $v0,$f2
    nop
    cvt.s.w $f2,$f2
    nop
    nop
    nop
  .endif
.else
  .if XP_RETAIL
    bne $v0,$zero,.Lother_size
    div.s $f0,$f2,$f0
    cvt.d.s $f0,$f0
    j XP_RETAIL_WEIGHTED
    mul.d $f0,$f0,$f8
.Lother_size:
    bnel $v0,$a3,.Lend
    addiu $v1,$v1,1
    cvt.d.s $f0,$f0
    mul.d $f0,$f0,$f6
    cvt.s.d $f0,$f0
    addiu $v1,$v1,1
.Lend:
  .else
    div.s $f0,$f2,$f0
    cvt.d.s $f0,$f0
    beql $v0,$zero,.Lweighted
    mul.d $f0,$f0,$f8
    beql $v0,$a3,.Lweighted
    mul.d $f0,$f0,$f6
    nop
.Lweighted:
    cvt.s.d $f0,$f0
    mul.s $f0,$f0,$f4
    addiu $v1,$v1,1
    nop
  .endif
.endif
.endif
