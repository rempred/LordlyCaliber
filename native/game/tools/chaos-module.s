# Maintained source adapted from parent tools/build_cf_army_counter_modregion_rom.py
# and Decomp e50fe500 tools/lordly_phase3_native_payload.py. Assembled at user export.
# Army final-task insertion: guard owner, bounded task, trailer and roster marker.
# Shift the original tail, insert the authored plate and substitute live CF digits.
    .word 0x4f424346,0x00000004,0x00000100,0x00000398
    .word 0x000003c0,0x00000700,0x00000a00,0x00000298
    .word 0x00000028,0x00000340,0x00000248,0x00000030
.org 0x100
    addiu $sp,$sp,-32
    sw $ra,0x0($sp)
    sw $a0,0x4($sp)
    sw $a1,0x8($sp)
    lui $t0,0x8019
    ori $t0,$t0,0x6af8
    lw $t1,0x0($t0)
    lui $t2,0x801f
    ori $t2,$t2,0x1550
    bne $t1,$t2,call_original
    nop
relocatable_task_pointer:
    lw $t2,0x30($a0)
    sll $t2,$t2,0x3
    srl $t2,$t2,0x3
    andi $t4,$t2,0x7
    bne $t4,$zero,call_original
    nop
    srl $t5,$t2,0x10
    sltiu $t4,$t5,0x20
    bne $t4,$zero,call_original
    nop
    sltiu $t4,$t5,0x30
    beq $t4,$zero,call_original
    nop
    lui $t0,0xa000
    addu $t0,$t0,$t2
    sw $t0,0xc($sp)
    lui $t3,0x8000
    addu $t3,$t3,$t2
    addiu $t3,$t3,-23584
    sw $t3,0x1c($sp)
target_task:
    lw $t1,0x34($a0)
    sltiu $t5,$t1,0x1198
    bne $t5,$zero,call_original
    nop
    sltiu $t5,$t1,0x3401
    beq $t5,$zero,call_original
    nop
    sw $t1,0x14($sp)
    addiu $t3,$t1,-752
    lw $t0,0xc($sp)
    addu $t4,$t0,$t3
    lw $t1,0x0($t4)
    lui $t2,0xed06
    ori $t2,$t2,0x4324
    bne $t1,$t2,try_extended_tail
    nop
    lw $t1,0x4($t4)
    lui $t2,0x1b
    ori $t2,$t2,0x437c
    beq $t1,$t2,compact_tail_found
    nop
try_extended_tail:
    lw $t1,0x14($sp)
    addiu $t3,$t1,-2320
    lw $t0,0xc($sp)
    addu $t4,$t0,$t3
    lw $t1,0x0($t4)
    lui $t2,0xed06
    ori $t2,$t2,0x4324
    bne $t1,$t2,call_original
    nop
    lw $t1,0x4($t4)
    lui $t2,0x1b
    ori $t2,$t2,0x437c
    bne $t1,$t2,call_original
    nop
    addiu $t6,$zero,0x244
    j tail_found
    nop
compact_tail_found:
    addiu $t6,$zero,0xbc
tail_found:
    sw $t3,0x10($sp)
    sw $t6,0x18($sp)
    lw $t0,0xc($sp)
    lw $t4,0x10($sp)
    addu $t4,$t0,$t4
    lw $t1,-2184($t4)
    lui $t2,0x100
    ori $t2,$t2,0x4008
    bne $t1,$t2,call_original
    nop
    lw $t1,-2180($t4)
    lw $t3,0x1c($sp)
    bne $t1,$t3,call_original
    nop
    lw $t1,0x14($sp)
    addiu $t1,$t1,0x32f
    andi $t1,$t1,0xffc0
    sw $t1,0x34($a0)
    lui $t0,0x8019
    ori $t0,$t0,0x36a9
    lbu $t6,0x0($t0)
    addiu $t8,$zero,0x64
    sltiu $t5,$t6,0x64
    bne $t5,$zero,under_100
    nop
    addiu $t9,$zero,0x1
    addiu $t7,$zero,0x0
    addiu $t6,$zero,0x0
    j digits_ready
    nop
under_100:
    addiu $t9,$zero,0x0
    addiu $t7,$zero,0x0
tens_loop:
    sltiu $t5,$t6,0xa
    bne $t5,$zero,digits_ready
    nop
    addiu $t6,$t6,-10
    addiu $t7,$t7,0x1
    j tens_loop
    nop
digits_ready:
    lui $t0,0x8042
    ori $t0,$t0,0x398
    sll $t4,$t9,0x2
    addu $t4,$t4,$t0
    lw $t1,0x0($t4)
    sll $t4,$t7,0x2
    addu $t4,$t4,$t0
    lw $t2,0x0($t4)
    sll $t4,$t6,0x2
    addu $t4,$t4,$t0
    lw $t3,0x0($t4)
    lw $t0,0xc($sp)
    lw $t4,0x14($sp)
    addu $t4,$t0,$t4
    addiu $t4,$t4,-4
    addiu $t5,$t4,0x2f0
    lw $t6,0x18($sp)
move_tail_loop:
    lw $t8,0x0($t4)
    sw $t8,0x0($t5)
    addiu $t4,$t4,-4
    addiu $t5,$t5,-4
    addiu $t6,$t6,-1
    bne $t6,$zero,move_tail_loop
    nop
    lw $t0,0xc($sp)
    lw $t5,0x10($sp)
    addu $t5,$t0,$t5
    lui $t4,0x8042
    ori $t4,$t4,0x3c0
    addiu $t6,$zero,0xbc
copy_payload_loop:
    lw $t8,0x0($t4)
    sw $t8,0x0($t5)
    addiu $t4,$t4,0x4
    addiu $t5,$t5,0x4
    addiu $t6,$t6,-1
    bne $t6,$zero,copy_payload_loop
    nop
    lw $t0,0xc($sp)
    lw $t5,0x10($sp)
    addu $t0,$t0,$t5
    sw $t1,0x27c($t0)
    sw $t2,0x2a4($t0)
    sw $t3,0x2cc($t0)
call_original:
    lw $a0,0x4($sp)
    lw $a1,0x8($sp)
    jal CF_SUBMIT_TASK
    nop
    lw $ra,0x0($sp)
    addiu $sp,$sp,0x20
    j CF_RETURN
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    .word 0x00000340,0x00000200,0x00e00200,0x01c00200
    .word 0x02a00200,0x03800200,0x04600200,0x05400200
    .word 0x06200200,0x07000200
# F3DEX2 plate display list. Existing Army segment 6 supplies parchment/borders.
    .word 0xed000000,0x004fc3bc,0xe7000000,0x00000000
    .word 0xe7000000,0x00000000,0xde000000,0x060035e8
    .word 0x01004008,0x804206c0,0x06000204,0x00000406
    .word 0xe7000000,0x00000000,0xde000000,0x06003690
    .word 0xfd500000,0x060034c0,0xf5500000,0x0708c240
    .word 0xe6000000,0x00000000,0xf3000000,0x0701f800
    .word 0xe7000000,0x00000000,0xf5400200,0x0008c240
    .word 0xf2000000,0x0003c01c,0xe7000000,0x00000000
    .word 0xe44c8070,0x003c0050,0xe1000000,0x01000000
    .word 0xf1000000,0x04000400,0xe7000000,0x00000000
    .word 0xe43e00c0,0x003c0050,0xe1000000,0x01000000
    .word 0xf1000000,0x04000400,0xe7000000,0x00000000
    .word 0xfd500000,0x06003500,0xf5500000,0x0708c240
    .word 0xe6000000,0x00000000,0xf3000000,0x0701f800
    .word 0xe7000000,0x00000000,0xf5400200,0x0008c240
    .word 0xf2000000,0x0003c01c,0xe7000000,0x00000000
    .word 0xe44d80c4,0x004b8050,0xe1000000,0x01000000
    .word 0xf1000000,0x04000400,0xe7000000,0x00000000
    .word 0xfd500000,0x06003580,0xf5500000,0x0708c240
    .word 0xe6000000,0x00000000,0xf3000000,0x0701f800
    .word 0xe7000000,0x00000000,0xf5400200,0x0008c240
    .word 0xf2000000,0x0003c01c,0xe7000000,0x00000000
    .word 0xe44c80d0,0x003c00b0,0xe1000000,0x01000000
    .word 0xf1000000,0x04000400,0xe7000000,0x00000000
    .word 0xfd500000,0x06003540,0xf5500000,0x0708c240
    .word 0xe6000000,0x00000000,0xf3000000,0x0701f800
    .word 0xe7000000,0x00000000,0xf5400200,0x0008c240
    .word 0xf2000000,0x0003c01c,0xe7000000,0x00000000
    .word 0xe44d80d0,0x004b80b0,0xe1000000,0x01000000
    .word 0xf1000000,0x04000400,0xe7000000,0x00000000
    .word 0xe7000000,0x00000000,0xe7000000,0x00000000
    .word 0xed3d4064,0x004c40bc,0xe7000000,0x00000000
    .word 0xfa000000,0xffffffff,0xe3000c00,0x00000000
    .word 0xe3001201,0x00000000,0xe200001c,0x0f0a7008
    .word 0xfc119623,0xff2fffff,0xde000000,0x060047a0
    .word 0xfd500000,0x801ffcd8,0xf5500000,0x07000000
    .word 0xe6000000,0x00000000,0xf3000000,0x073ff200
    .word 0xe7000000,0x00000000,0xf5400800,0x00000000
    .word 0xf2000000,0x000fc0fc,0xe7000000,0x00000000
    .word 0xe44700a4,0x00454080,0xe1000000,0x00000000
    .word 0xf1000000,0x04000400,0xe7000000,0x00000000
    .word 0xe7000000,0x00000000,0xe448c0a4,0x00470080
    .word 0xe1000000,0x00000000,0xf1000000,0x04000400
    .word 0xe7000000,0x00000000,0xe7000000,0x00000000
    .word 0xe44a80a4,0x0048c080,0xe1000000,0x00000000
    .word 0xf1000000,0x04000400,0xe7000000,0x00000000
    .word 0xe7000000,0x00000000,0xde000000,0x80420880
.org 0x6c0
# Four repositioned plate vertices (16 bytes each).
    .word 0xff6e0048,0x00000000,0x000005c0,0xffffffff
    .word 0xff6e0060,0x00000000,0x00000000,0xffffffff
    .word 0xffac0060,0x00000000,0x0f400000,0xffffffff
    .word 0xffac0048,0x00000000,0x0f4005c0,0xffffffff
.org 0x700
# ROM-derived title CI4 bytes are appended here by the export compiler.
