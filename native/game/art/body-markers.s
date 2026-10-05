# Replace only the two equipment-marker scans inside the original constructor.
# z64 [00205FD8,002060B4), live [801C2B48,801C2C24).
# $s3 is the body resource record; caller-saved registers are disposable here.
# LC_WIDE=0 restores the original loops. LC_WIDE=1 also reads opcode21 tokens,
# whose two bytes are low then high. The surrounding loader and heap stay intact.
# The rebuilt scan checks that poses38/88 exist before reading their offsets.
lordly_body_markers:
.if LC_WIDE
    lw $t0, 0x44($s3)
    ori $t1, $zero, 0xffff
    sh $t1, 0xa8($s3)
    sh $t1, 0xaa($s3)
    sh $t1, 0xac($s3)
    sh $t1, 0xae($s3)
    lw $t7, 0x4c($s3)
    sltiu $v0, $t7, 0x27
    bne $v0, $zero, markers_done
    nop
    lw $t1, 0x98($t0)
    addu $t1, $t0, $t1
    addiu $t2, $s3, 0xa8
    addiu $t3, $zero, 1
    addiu $t4, $zero, 21
    lui $t5, %hi(lc_pose_widths)
    addiu $t5, $t5, %lo(lc_pose_widths)
wide_start:
    lbu $t6, 0($t1)
    addiu $t1, $t1, 1
    beq $t6, $zero, wide_end
    nop
wide_scan:
    lbu $v0, 0($t1)
    beq $v0, $t3, wide_narrow
    nop
    bne $v0, $t4, wide_advance
    nop
    lbu $v1, 2($t1)
    sll $v1, $v1, 8
    lbu $a0, 1($t1)
    or $v1, $v1, $a0
    beq $zero, $zero, wide_store
    nop
wide_narrow:
    lbu $v1, 1($t1)
wide_store:
    lhu $a0, 0($t2)
    sh $a0, 2($t2)
    sh $v1, 0($t2)
wide_advance:
    addu $a0, $t5, $v0
    lbu $a0, 0($a0)
    addiu $t6, $t6, -1
    bne $t6, $zero, wide_scan
    addu $t1, $t1, $a0
wide_end:
    addiu $a0, $s3, 0xa8
    bne $t2, $a0, markers_done
    lw $v0, 0x4c($s3)
    sltiu $v0, $v0, 0x59
    bne $v0, $zero, markers_done
    nop
    lw $t1, 0x160($t0)
    addu $t1, $t0, $t1
    beq $zero, $zero, wide_start
    addiu $t2, $t2, 4
.else
    lw $a2, 0x44($s3)
    lw $v0, 0x98($a2)
    addu $a0, $zero, $zero
    addu $v1, $a2, $v0
    lbu $a1, 0($v1)
    addiu $v1, $v1, 1
    ori $v0, $zero, 0xffff
    sh $v0, 0xac($s3)
    sh $v0, 0xae($s3)
    sh $v0, 0xa8($s3)
    beq $a1, $zero, narrow_second
    sh $v0, 0xaa($s3)
    addiu $a3, $zero, 1
narrow_first_scan:
    lbu $v0, 0($v1)
    bne $v0, $a3, narrow_first_advance
    nop
    lhu $v0, 0xa8($s3)
    sh $v0, 0xaa($s3)
    lbu $v0, 1($v1)
    sh $v0, 0xa8($s3)
    lbu $v0, 0($v1)
narrow_first_advance:
    lui $at, 0x801d
    addu $at, $at, $v0
    lbu $v0, -4384($at)
    addiu $a0, $a0, 1
    addu $v1, $v1, $v0
    sltu $v0, $a0, $a1
    bne $v0, $zero, narrow_first_scan
    nop
narrow_second:
    lw $v0, 0x4c($s3)
    sltiu $v0, $v0, 0x58
    bne $v0, $zero, markers_done
    addu $a0, $zero, $zero
    lw $v0, 0x160($a2)
    addu $v1, $a2, $v0
    lbu $a1, 0($v1)
    beq $a1, $zero, markers_done
    addiu $v1, $v1, 1
    addiu $a2, $zero, 1
narrow_second_scan:
    lbu $v0, 0($v1)
    bne $v0, $a2, narrow_second_advance
    nop
    lhu $v0, 0xac($s3)
    sh $v0, 0xae($s3)
    lbu $v0, 1($v1)
    sh $v0, 0xac($s3)
    lbu $v0, 0($v1)
narrow_second_advance:
    lui $at, 0x801d
    addu $at, $at, $v0
    lbu $v0, -4384($at)
    addiu $a0, $a0, 1
    addu $v1, $v1, $v0
    sltu $v0, $a0, $a1
    bne $v0, $zero, narrow_second_scan
    nop
.endif
markers_done:
.if LC_WIDE
    .space 220-(.-lordly_body_markers)
.endif
