# Maintained US Rev0 source adapted from parent tools/build_high_attack_stream_shift_rom.py.
# Version 23 repairs loader identity and reconciles known references at each active insert.
# Experimental; Project64 Interpreter only. No per-site cache experiment.
.word 0x4f424853,23,0x80450000,0xffff9810,546,0x940,0x801b0000,0x80210000
.word 0,0

.org 0x40
# entry
    lui $t0,32797
    lw $t1,-5952($t0)
    lui $t2,32837
    ori $t2,$t2,0
    beq $t1,$t2,entry_patch_setup
    nop
    lui $t8,32836
    lw $t5,32($t8)
    bne $t5,$zero,entry_patch_setup
    nop
# A freshly loaded module can coexist with a previously shifted overlay.
    lw $t9,16($t8)
    ori $t1,$t8,0x940
entry_probe:
    beq $t9,$zero,entry_threshold
    nop
    lw $t0,0($t1)
    lw $t2,0($t0)
    lw $t6,4($t1)
    andi $t3,$t6,65535
    addiu $t3,$t3,-28672
    andi $t3,$t3,65535
    lui $t4,65535
    and $t6,$t6,$t4
    or $t6,$t6,$t3
    beq $t2,$t6,entry_patch_setup
    nop
    addiu $t1,$t1,8
    addiu $t9,$t9,-1
    beq $zero,$zero,entry_probe
    nop
entry_threshold:
# The advance helper can outlive a partial overlay reload on its own.
    lui $t0,0x801e
    lw $t1,-26976($t0)
    lui $t2,0x0811
    ori $t2,$t2,0x0100
    beq $t1,$t2,entry_patch_setup
    nop
    lui $t0,0x801d
    lw $t1,-5952($t0)
    lw $t3,2068($t1)
    addu $t6,$t3,$s3
    sltiu $t4,$t6,2049
    bne $t4,$zero,entry_mirror_vanilla
    nop
# Re-check exact expected words, including partially reloaded overlays.
entry_patch_setup:
    lui $t8,32836
    lw $t9,16($t8)
    ori $t1,$t8,2368
entry_patch_loop:
    blez $t9,entry_patch_done
    nop
    lw $t0,0($t1)
    lw $t2,0($t0)
    lw $t6,4($t1)
    bne $t2,$t6,entry_patch_next
    nop
    andi $t3,$t2,65535
    sltiu $t4,$t3,2064
    bne $t4,$zero,entry_patch_next
    nop
    sltiu $t4,$t3,2304
    bne $t4,$zero,entry_patch_apply
    nop
    sltiu $t4,$t3,24560
    bne $t4,$zero,entry_patch_next
    nop
    sltiu $t4,$t3,24800
    beq $t4,$zero,entry_patch_next
    nop
entry_patch_apply:
    addiu $t3,$t3,-28672
    lui $t6,65535
    and $t2,$t2,$t6
    andi $t3,$t3,65535
    or $t2,$t2,$t3
    sw $t2,0($t0)
entry_patch_next:
    addiu $t1,$t1,8
    addiu $t9,$t9,-1
    beq $zero,$zero,entry_patch_loop
    nop
entry_patch_done:
    lui $t0,32797
    ori $t0,$t0,38560
    lw $t1,0($t0)
    lui $t2,15366
    ori $t2,$t2,32797
    bne $t1,$t2,entry_helper_patch_done
    nop
    lw $t1,4($t0)
    lui $t2,36038
    ori $t2,$t2,59584
    bne $t1,$t2,entry_helper_patch_done
    nop
    lui $t2,2065
    ori $t2,$t2,256
    sw $t2,0($t0)
    sw $zero,4($t0)
entry_helper_patch_done:
    ori $t5,$zero,1
    sw $t5,32($t8)
entry_copy_layout:
# Never recopy the old positive trailer over an already relocated context.
    lui $t0,0x801d
    lw $t1,-5952($t0)
    lui $t2,0x8045
    beq $t1,$t2,entry_mirror_shifted
    nop
    lui $t0,32797
    lw $t1,-5956($t0)
    lui $t2,32836
    ori $t2,$t2,43040
    addu $t4,$t1,$zero
    addu $t5,$t2,$zero
    addiu $t6,$t1,24724
entry_owner_copy_loop:
    beq $t4,$t6,entry_context_copy_setup
    nop
    lw $t7,0($t4)
    sw $t7,0($t5)
    addiu $t4,$t4,4
    addiu $t5,$t5,4
    beq $zero,$zero,entry_owner_copy_loop
    nop
entry_context_copy_setup:
    lui $t0,32797
    lw $t1,-5952($t0)
    lui $t2,32837
    ori $t2,$t2,0
    addu $t4,$t1,$zero
    addu $t5,$t2,$zero
    addiu $t6,$t1,2064
entry_copy_head:
    beq $t4,$t6,entry_zero_gap
    nop
    lw $t7,0($t4)
    sw $t7,0($t5)
    addiu $t4,$t4,4
    addiu $t5,$t5,4
    beq $zero,$zero,entry_copy_head
    nop
entry_zero_gap:
    addiu $t6,$t2,2320
entry_zero_loop:
    beq $t5,$t6,entry_copy_trailer
    nop
    sw $zero,0($t5)
    addiu $t5,$t5,4
    beq $zero,$zero,entry_zero_loop
    nop
entry_copy_trailer:
    addiu $t4,$t1,2064
    addiu $t5,$t2,-26608
    addiu $t6,$t1,2320
entry_trailer_loop:
    beq $t4,$t6,entry_install_ptr
    nop
    lw $t7,0($t4)
    sw $t7,0($t5)
    addiu $t4,$t4,4
    addiu $t5,$t5,4
    beq $zero,$zero,entry_trailer_loop
    nop
entry_install_ptr:
    lui $t0,32797
    lui $t3,32836
    ori $t3,$t3,43040
    sw $t3,-5956($t0)
    sw $t2,-5952($t0)
    lui $t8,32836
    sw $zero,36($t8)
entry_mirror_shifted:
    lui $v0,32797
    lw $v0,-5952($v0)
    addu $s2,$s1,$s0
    lw $a2,-26604($v0)
    lui $t8,32836
    lw $t9,36($t8)
    addu $t6,$a2,$s3
    sltu $t7,$t9,$t6
    beq $t7,$zero,entry_highwater_done
    nop
    sw $t6,36($t8)
entry_highwater_done:
    j HA_801D9A88
    nop
entry_mirror_vanilla:
    lui $v0,32797
    lw $v0,-5952($v0)
    addu $s2,$s1,$s0
    lw $a2,2068($v0)
    j HA_801D9A88
    nop

.org 0x400
# advance
    lui $v1,32797
    lw $a2,-5952($v1)
    lw $a1,-26604($a2)
    lui $t4,32836
    lw $t4,36($t4)
    sltu $t0,$a1,$t4
    beq $t0,$zero,advance_advance_len_ready
    nop
    addu $a1,$t4,$zero
advance_advance_len_ready:
    sltu $v0,$a0,$a1
    beq $v0,$zero,advance_not_found
    addu $v1,$a2,$a0
    lbu $v0,16($v1)
    ori $t0,$zero,22
    beq $v0,$t0,advance_type_16
    ori $t0,$zero,56
    beq $v0,$t0,advance_type_38
    nop
    beq $v0,$zero,advance_scan_zero
    nop
    lui $t1,32798
    ori $t1,$t1,23664
    addu $t1,$t1,$v0
    lbu $t2,0($t1)
    ori $t0,$zero,255
    beq $v0,$t0,advance_scan_zero
    nop
    beq $t2,$zero,advance_not_found
    nop
    addu $v0,$a0,$t2
    beq $zero,$zero,advance_check
    nop
advance_type_16:
    lbu $t2,19($v1)
    addiu $t2,$t2,4
    addu $v0,$a0,$t2
    beq $zero,$zero,advance_check
    nop
advance_type_38:
    lbu $t2,19($v1)
    beq $t2,$zero,advance_not_found
    nop
    addu $v0,$a0,$t2
    beq $zero,$zero,advance_check
    nop
advance_scan_zero:
    addiu $t3,$a0,1
advance_scan_loop:
    sltu $t0,$t3,$a1
    beq $t0,$zero,advance_not_found
    addu $v1,$a2,$t3
    lbu $t2,16($v1)
    bne $t2,$zero,advance_found_zero_scan
    nop
    addiu $t3,$t3,1
    beq $zero,$zero,advance_scan_loop
    nop
advance_found_zero_scan:
    addu $v0,$t3,$zero
    jr $ra
    nop
advance_check:
    sltu $t0,$v0,$a1
    bne $t0,$zero,advance_return
    nop
advance_not_found:
    lui $t0,32797
    ori $t0,$t0,10500
    beq $ra,$t0,advance_phase_handoff_skip
    nop
    lui $t0,32837
    ori $t0,$t0,0
    bne $a2,$t0,advance_phase_handoff_skip
    nop
    sltiu $t1,$a1,2048
    bne $t1,$zero,advance_phase_handoff_skip
    nop
    addiu $t1,$a0,3
    sltu $t2,$t1,$a1
    bne $t2,$zero,advance_phase_handoff_skip
    nop
    lbu $t1,-26524($a2)
    ori $t2,$zero,1
    beq $t1,$t2,advance_phase_handoff_phase_ok
    nop
    bne $t1,$zero,advance_phase_handoff_skip
    nop
advance_phase_handoff_phase_ok:
    lui $t1,32780
    lhu $t2,19494($t1)
    ori $t3,$zero,65535
    bne $t2,$t3,advance_phase_handoff_skip
    nop
    ori $t1,$zero,5
    sb $t1,-26524($a2)
    sw $zero,-26520($a2)
advance_phase_handoff_skip:
    addiu $v0,$zero,-1
advance_return:
    jr $ra
    nop

.org 0x620
# reset
    addiu $sp,$sp,-8
    sw $ra,4($sp)
    ori $t0,$zero,68
    beq $a0,$t0,reset_skip_helper
    nop
    jal HA_801AD974
    nop
reset_skip_helper:
    lw $ra,4($sp)
    addiu $sp,$sp,8
    jr $ra
    nop

.org 0x680
# class02
    beq $v0,$zero,class02_skip_slot
    nop
    lw $a0,72($v1)
    jr $ra
    nop
class02_skip_slot:
    addiu $s3,$s3,1
    j HA_801DC15C
    nop

.org 0x700
# final
    addiu $sp,$sp,-32
    sw $t1,0($sp)
    sw $t2,4($sp)
    sw $t3,8($sp)
    sw $t4,12($sp)
    sw $t5,16($sp)
    sw $t6,20($sp)
    sw $t7,24($sp)
    lui $t1,32797
    lw $t2,-5952($t1)
    lui $t3,32837
    ori $t3,$t3,0
    bne $t2,$t3,final_handoff_done
    nop
    lw $t4,-26604($t2)
    sltiu $t5,$t4,2048
    bne $t5,$zero,final_handoff_done
    nop
    lw $t5,-26608($t2)
    addiu $t5,$t5,3
    sltu $t6,$t5,$t4
    bne $t6,$zero,final_handoff_done
    nop
    lbu $t5,-26524($t2)
    ori $t6,$zero,1
    beq $t5,$t6,final_handoff_phase_ok
    nop
    bne $t5,$zero,final_handoff_done
    nop
final_handoff_phase_ok:
    lui $t5,32780
    lhu $t6,19494($t5)
    ori $t7,$zero,65535
    bne $t6,$t7,final_handoff_done
    nop
    ori $t5,$zero,5
    sb $t5,-26524($t2)
    sw $zero,-26520($t2)
final_handoff_done:
    lw $t1,0($sp)
    lw $t2,4($sp)
    lw $t3,8($sp)
    lw $t4,12($sp)
    lw $t5,16($sp)
    lw $t6,20($sp)
    lw $t7,24($sp)
    addiu $sp,$sp,32
    ori $t0,$zero,1
    lui $at,32797
    sw $zero,-5948($at)
    j HA_801DDF50
    sw $t0,684($sp)

.org 0x7d0
# class17
    beq $v0,$zero,class17_skip
    nop
    addu $s0,$v0,$zero
    addiu $s1,$s0,210
    j HA_801DBEB8
    nop
class17_skip:
    j HA_801DDF10
    nop

.org 0x820
# cleanup
    lui $t0,32836
    ori $t0,$t0,43040
    beq $a0,$t0,cleanup_skip_free
    nop
    j HA_800712C4
    nop
cleanup_skip_free:
    jr $ra
    nop

.org 0x900
# interrupt
    lui $t0,32797
    lw $t1,-5952($t0)
    lui $t0,32837
    ori $t0,$t0,0
    bne $t1,$t0,interrupt_reset_candidate
    ori $t0,$zero,255
    beq $a1,$zero,interrupt_continue_scan
    nop
    beq $a1,$t0,interrupt_continue_scan
    nop
interrupt_reset_candidate:
    addiu $s2,$zero,-1
interrupt_continue_scan:
    j HA_801D28FC
    nop

.org 0x940
# Guard data: qualified live-overlay PCs and expected original words.
.word 0x801b00f4,0x8c50607c
.word 0x801b015c,0x8c44607c
.word 0x801b0168,0xac40607c
.word 0x801b0bc0,0x8c506080
.word 0x801b247c,0x8cc26080
.word 0x801b24a0,0x8cc36080
.word 0x801bcc28,0x9062082c
.word 0x801bcc34,0xa062082c
.word 0x801bcc54,0x9062082d
.word 0x801bcc60,0xa062082d
.word 0x801bd9a8,0x8c646080
.word 0x801bdb34,0x90c2608c
.word 0x801bdb58,0x90c3608b
.word 0x801bdce4,0xa883082f
.word 0x801bdce8,0xb8830832
.word 0x801bdcec,0xa8860833
.word 0x801bdcf0,0xb8860836
.word 0x801bdcf4,0xa8870837
.word 0x801bdcf8,0xb887083a
.word 0x801bdcfc,0xa888083b
.word 0x801bdd00,0xb888083e
.word 0x801bdd18,0xa883083f
.word 0x801bdd1c,0xb8830842
.word 0x801bdd20,0xa8860843
.word 0x801bdd24,0xb8860846
.word 0x801bdd28,0xa0870847
.word 0x801bdd70,0xa8830848
.word 0x801bdd74,0xb883084b
.word 0x801bdd78,0xa885084c
.word 0x801bdd7c,0xb885084f
.word 0x801bdd80,0xa8860850
.word 0x801bdd84,0xb8860853
.word 0x801bdd88,0xa8870854
.word 0x801bdd8c,0xb8870857
.word 0x801bdda4,0xa8830858
.word 0x801bdda8,0xb883085b
.word 0x801bddac,0xa885085c
.word 0x801bddb0,0xb885085f
.word 0x801bddb4,0xa0860860
.word 0x801bddc4,0x2453082f
.word 0x801bddc8,0x24550834
.word 0x801bde78,0x24530848
.word 0x801bde7c,0x2454084d
.word 0x801bdf68,0x24a5082f
.word 0x801bdf84,0x24a50848
.word 0x801ca9fc,0x90426084
.word 0x801cadfc,0x90426084
.word 0x801cb320,0x90426084
.word 0x801cb658,0x90426084
.word 0x801cc100,0x8c626080
.word 0x801cc108,0xac626080
.word 0x801d0ea4,0xac916050
.word 0x801d0ea8,0xac906054
.word 0x801d1ae8,0x90440836
.word 0x801d1cf0,0x9044084f
.word 0x801d20a0,0xac400814
.word 0x801d20a4,0xa060608d
.word 0x801d20e4,0x94426068
.word 0x801d2298,0xa4626068
.word 0x801d22e0,0xa4626068
.word 0x801d2348,0xa4406068
.word 0x801d2368,0xa4626068
.word 0x801d23a4,0x8c656060
.word 0x801d23a8,0x8c646064
.word 0x801d23bc,0xa4606068
.word 0x801d23cc,0xa4626068
.word 0x801d23d8,0x9462606a
.word 0x801d23e8,0xa4626068
.word 0x801d23f4,0x90836087
.word 0x801d240c,0xa4826068
.word 0x801d2418,0x90836087
.word 0x801d2424,0xa4806068
.word 0x801d2430,0x90836087
.word 0x801d243c,0xa4806068
.word 0x801d2448,0x94426068
.word 0x801d25e0,0xa0516044
.word 0x801d25ec,0xac406048
.word 0x801d264c,0xac400814
.word 0x801d265c,0x9051082e
.word 0x801d2668,0xa051082e
.word 0x801d2720,0x8c420814
.word 0x801d2754,0xac400814
.word 0x801d2764,0x9051082e
.word 0x801d2770,0xa051082e
.word 0x801d280c,0x8c440810
.word 0x801d2830,0x90420819
.word 0x801d2930,0x90430819
.word 0x801d2944,0x8c620810
.word 0x801d295c,0x90436072
.word 0x801d2994,0x90626086
.word 0x801d29a0,0x90626072
.word 0x801d29ac,0xa043606e
.word 0x801d29c8,0xa0406073
.word 0x801d29dc,0xa062608c
.word 0x801d29e8,0xa040606e
.word 0x801d29f4,0xa0406072
.word 0x801d2a00,0xa040608c
.word 0x801d2a38,0x90c26072
.word 0x801d2a40,0x90a3606e
.word 0x801d2a80,0xa0a2606e
.word 0x801d2a90,0xa040608b
.word 0x801d2abc,0x90436072
.word 0x801d2ac8,0xa043606e
.word 0x801d2ad4,0xa043608b
.word 0x801d2b1c,0xa040608c
.word 0x801d2b28,0x90c3608b
.word 0x801d2b34,0x90c36072
.word 0x801d2b3c,0x9062606e
.word 0x801d2b48,0xa062606e
.word 0x801d2b54,0x90c36072
.word 0x801d2b68,0xa062606e
.word 0x801d2be4,0xa0a0606e
.word 0x801d2bf8,0xa0406072
.word 0x801d2c2c,0x90c26073
.word 0x801d2c48,0x90c36073
.word 0x801d2c5c,0xa0c26073
.word 0x801d2c60,0xa0c06073
.word 0x801d2c84,0x90d06073
.word 0x801d2ca8,0xa040606e
.word 0x801d2cc0,0xa0406072
.word 0x801d2d3c,0x90436073
.word 0x801d2d9c,0x90626072
.word 0x801d2db0,0xa0626072
.word 0x801d2e0c,0x90436073
.word 0x801d2e5c,0xa040606e
.word 0x801d2e74,0xa0406072
.word 0x801d2eb8,0xa0626087
.word 0x801d2eec,0xac400814
.word 0x801d2efc,0x9051082e
.word 0x801d2f08,0xa051082e
.word 0x801d301c,0x90c26072
.word 0x801d3030,0xa0c26072
.word 0x801d3044,0xa0c2606e
.word 0x801d3070,0x90436073
.word 0x801d30a0,0x90426073
.word 0x801d3120,0xa0a2606e
.word 0x801d31a4,0x90436072
.word 0x801d31b4,0xa043606e
.word 0x801d3248,0x90626072
.word 0x801d3284,0x90626072
.word 0x801d32b0,0x90626072
.word 0x801d330c,0xa4626076
.word 0x801d3318,0xa0406072
.word 0x801d332c,0x90626072
.word 0x801d3340,0xa060606e
.word 0x801d33f0,0x90626072
.word 0x801d3424,0x90626072
.word 0x801d3428,0xa4606076
.word 0x801d3434,0xa0626072
.word 0x801d34c8,0x90626072
.word 0x801d34dc,0xa0626072
.word 0x801d34f0,0xa062606e
.word 0x801d3628,0x90626072
.word 0x801d3630,0xa0626072
.word 0x801d363c,0xa0406072
.word 0x801d3648,0x90436072
.word 0x801d3654,0xa040606e
.word 0x801d368c,0xac400814
.word 0x801d369c,0x9051082e
.word 0x801d36a8,0xa051082e
.word 0x801d41c8,0x8c426080
.word 0x801d43c0,0x90436072
.word 0x801d43c8,0x9044606e
.word 0x801d4468,0x90436073
.word 0x801d47f4,0x90846073
.word 0x801d4824,0x90466073
.word 0x801d4870,0x90436073
.word 0x801d48f4,0x8c626048
.word 0x801d4954,0xa0606089
.word 0x801d4964,0xa040608a
.word 0x801d4988,0x90626089
.word 0x801d4990,0xa0626089
.word 0x801d49bc,0x9062608a
.word 0x801d49c4,0xa062608a
.word 0x801d49d0,0x8c436048
.word 0x801d4a1c,0x8c836048
.word 0x801d4a4c,0x8c836048
.word 0x801d4a7c,0x8c82604c
.word 0x801d4a80,0x8c836048
.word 0x801d4a90,0xac82604c
.word 0x801d4abc,0xac400814
.word 0x801d4acc,0x9051082e
.word 0x801d4ad8,0xa051082e
.word 0x801d4b6c,0x8c826048
.word 0x801d4b7c,0x8c82604c
.word 0x801d4b8c,0xac82604c
.word 0x801d4be4,0x8c626048
.word 0x801d4c08,0x90626072
.word 0x801d4c10,0x9082606e
.word 0x801d4c44,0xa082606e
.word 0x801d4c54,0xa040608b
.word 0x801d4c74,0x90626072
.word 0x801d4c7c,0x9082606e
.word 0x801d4c9c,0xa083606e
.word 0x801d4ca8,0xa043608b
.word 0x801d4cbc,0x8c626048
.word 0x801d4cc4,0xac626048
.word 0x801d4d1c,0x8c646048
.word 0x801d4d50,0x8c646048
.word 0x801d4d78,0x8c636048
.word 0x801d4ddc,0x8c636048
.word 0x801d4fb0,0x90436087
.word 0x801d4fbc,0x9443606a
.word 0x801d504c,0x9442606a
.word 0x801d50d8,0x9442606a
.word 0x801d517c,0x8c626048
.word 0x801d51a8,0x8c626048
.word 0x801d51c0,0xac626048
.word 0x801d51cc,0x8c426048
.word 0x801d5340,0x8cc36048
.word 0x801d5370,0x8cc2604c
.word 0x801d537c,0xacc2604c
.word 0x801d53c4,0x8cc26048
.word 0x801d53e0,0x8cc26048
.word 0x801d53f8,0x8cc2604c
.word 0x801d5410,0xacc2604c
.word 0x801d543c,0x90436087
.word 0x801d5494,0xa462606a
.word 0x801d54a0,0x9442606a
.word 0x801d5534,0x94436068
.word 0x801d5574,0x8c456048
.word 0x801d5594,0x94a36068
.word 0x801d55b4,0x8ca56048
.word 0x801d55c8,0x8c656048
.word 0x801d55e4,0x8c436048
.word 0x801d5624,0x94636068
.word 0x801d5650,0x8c436048
.word 0x801d575c,0x94c46068
.word 0x801d58c4,0x8c446048
.word 0x801d5914,0x90426087
.word 0x801d5938,0x90436087
.word 0x801d5a94,0x90436087
.word 0x801d5abc,0x90436087
.word 0x801d5bec,0x90426087
.word 0x801d5c10,0x90426087
.word 0x801d5dac,0x8c626048
.word 0x801d5db4,0xac626048
.word 0x801d5e24,0x9062608d
.word 0x801d5e44,0x9042608e
.word 0x801d5e5c,0x9062608d
.word 0x801d61c0,0x8c436048
.word 0x801d62a4,0x94426068
.word 0x801d63e0,0x8c646048
.word 0x801d63e4,0xac606050
.word 0x801d63f0,0xac626048
.word 0x801d63fc,0xac626048
.word 0x801d6408,0x8c826050
.word 0x801d6418,0xac836050
.word 0x801d642c,0x8c826048
.word 0x801d6438,0xac826048
.word 0x801d6770,0x8c436048
.word 0x801d67b4,0xac626048
.word 0x801d67d8,0xa0626074
.word 0x801d67ec,0xac626048
.word 0x801d67f8,0xac446048
.word 0x801d6808,0xac406050
.word 0x801d681c,0x8c636050
.word 0x801d6a4c,0xa0626087
.word 0x801d6a74,0x8c836050
.word 0x801d6abc,0xac626048
.word 0x801d6ad0,0x8c836050
.word 0x801d6b18,0xac826048
.word 0x801d6b24,0x8c626050
.word 0x801d6b30,0xac626050
.word 0x801d6b68,0xac400814
.word 0x801d6b78,0x9051082e
.word 0x801d6b84,0xa051082e
.word 0x801d6c5c,0x8c836048
.word 0x801d6c94,0xac806050
.word 0x801d6c98,0xac926048
.word 0x801d6cd0,0x8c826050
.word 0x801d6cec,0x8c446050
.word 0x801d6dc0,0xac606054
.word 0x801d6dc4,0xac626048
.word 0x801d6fc8,0xac716058
.word 0x801d6fd0,0xac70605c
.word 0x801d6ff0,0x8c626050
.word 0x801d6ff8,0xac626050
.word 0x801d7010,0x8c426050
.word 0x801d7128,0x8c626050
.word 0x801d712c,0xac726048
.word 0x801d7134,0xac626050
.word 0x801d7140,0x8c626054
.word 0x801d7148,0xac626054
.word 0x801d7180,0x8c436048
.word 0x801d7184,0x8c516050
.word 0x801d7188,0x8c506054
.word 0x801d72d8,0x8c626048
.word 0x801d72e0,0xac626048
.word 0x801d7324,0x8c706048
.word 0x801d737c,0x8c826048
.word 0x801d7384,0xac836050
.word 0x801d7390,0xac826048
.word 0x801d7394,0x8c626050
.word 0x801d73a0,0xac626050
.word 0x801d7584,0x8c436050
.word 0x801d75d0,0xa0406088
.word 0x801d75e0,0xa0626087
.word 0x801d7614,0xac400814
.word 0x801d7624,0x9051082e
.word 0x801d7630,0xa051082e
.word 0x801d7708,0x8ca36048
.word 0x801d7798,0xac606050
.word 0x801d779c,0xac626048
.word 0x801d7820,0xac54605c
.word 0x801d784c,0x8ca26050
.word 0x801d7868,0x8c446050
.word 0x801d78bc,0x8c42605c
.word 0x801d78e4,0x8c42605c
.word 0x801d7a30,0xac606054
.word 0x801d7a34,0xac626048
.word 0x801d7a3c,0xac716058
.word 0x801d7a48,0x8c626050
.word 0x801d7a50,0xac626050
.word 0x801d7a5c,0x8c426050
.word 0x801d7a74,0x8c426050
.word 0x801d7b70,0x8c626054
.word 0x801d7b78,0xac626054
.word 0x801d85e8,0x90706044
.word 0x801d86a8,0x90836044
.word 0x801d86b4,0x90826072
.word 0x801d86bc,0x9042606e
.word 0x801d87a0,0x90626044
.word 0x801d87ac,0x90626072
.word 0x801d87c8,0x90426044
.word 0x801d8890,0x8c42604c
.word 0x801d88dc,0x90836044
.word 0x801d88e8,0x90826072
.word 0x801d88f0,0x9042606e
.word 0x801d8970,0x9463606a
.word 0x801d8a04,0x94426068
.word 0x801d8a2c,0x94436068
.word 0x801d8a88,0x90426089
.word 0x801d8ad0,0x90426089
.word 0x801d8b14,0x90426089
.word 0x801d8b60,0x9042608a
.word 0x801d8ba4,0x9042608a
.word 0x801d8be8,0x9042608a
.word 0x801d8cb0,0x9462606c
.word 0x801d8d7c,0xa453606c
.word 0x801d9528,0x90436044
.word 0x801d96a8,0x8cc20814
.word 0x801d972c,0x8c420814
.word 0x801d9764,0x8c420814
.word 0x801d978c,0x8c630814
.word 0x801d97c0,0x8c620814
.word 0x801d9844,0x8c620814
.word 0x801d987c,0x8ca20814
.word 0x801d98b4,0x8ca30814
.word 0x801d98dc,0xac440810
.word 0x801d99d8,0x8c420814
.word 0x801d9a0c,0x8c630814
.word 0x801d9a28,0x8c420814
.word 0x801d9a40,0x8c500814
.word 0x801d9a84,0x8c460814
.word 0x801d9aa0,0x8c620814
.word 0x801d9aac,0xac620814
.word 0x801d9acc,0x90420818
.word 0x801d9b0c,0x90420818
.word 0x801d9e8c,0x8c420814
.word 0x801da110,0x8cc20814
.word 0x801da140,0x8cc20814
.word 0x801da160,0x8c430814
.word 0x801da168,0xac430814
.word 0x801db930,0x8c830810
.word 0x801db9e8,0xa0620821
.word 0x801db9f8,0xa0480822
.word 0x801dba08,0xa0620820
.word 0x801dba18,0x90440821
.word 0x801dba58,0x90420820
.word 0x801dbb20,0xa0400824
.word 0x801dbb40,0x90620824
.word 0x801dbb4c,0xa0620824
.word 0x801dbb58,0x90430820
.word 0x801dbb94,0x90620824
.word 0x801dbb9c,0xa0620824
.word 0x801dbba8,0x90620823
.word 0x801dbbb4,0xa0620823
.word 0x801dbc20,0x90440820
.word 0x801dbdb0,0x90430820
.word 0x801dbdd4,0x90430820
.word 0x801dbdf8,0x90430820
.word 0x801dbefc,0x90420822
.word 0x801dbf98,0x90620819
.word 0x801dbfa4,0xa0650819
.word 0x801dc798,0x8c626060
.word 0x801dc7a4,0xac626060
.word 0x801dc7b0,0x8c626064
.word 0x801dc7b8,0xac626064
.word 0x801dc990,0x8c626060
.word 0x801dc99c,0xac626060
.word 0x801dc9a8,0x8c626064
.word 0x801dc9b0,0xac626064
.word 0x801dcca4,0x8c626060
.word 0x801dccb0,0xac626060
.word 0x801dccbc,0x8c626064
.word 0x801dccc8,0xac626064
.word 0x801dd004,0x9083608d
.word 0x801dd00c,0xa082608d
.word 0x801dd018,0xa062608e
.word 0x801dd8c8,0xa062082d
.word 0x801dd944,0xa082082c
.word 0x801dd970,0x8c420810
.word 0x801ddcf0,0x90620819
.word 0x801ddcfc,0xa0650819
.word 0x801ddd38,0x9083082e
.word 0x801ddd48,0xa082082e
.word 0x801ddf1c,0x8c440810
.word 0x801ddf2c,0xac620810
.word 0x801ddf64,0x90836044
.word 0x801ddf70,0x90826072
.word 0x801ddf78,0x9043606e
.word 0x801ddfe0,0x90426087
.word 0x801de050,0x90826044
.word 0x801de05c,0x90826072
.word 0x801de064,0x9043606e
.word 0x801de07c,0x90826072
.word 0x801de088,0xa043606e
.word 0x801de094,0x90836044
.word 0x801de0a4,0x90826072
.word 0x801de0ac,0x9042606e
.word 0x801de0bc,0x90826088
.word 0x801df444,0x90a30820
.word 0x801df464,0x90a20824
.word 0x801df470,0x8ca20814
.word 0x801df4c4,0x90a20823
.word 0x801df4e0,0x8ca20810
.word 0x801df528,0x8c820810
.word 0x801df54c,0x8ca30814
.word 0x801df5f0,0x90620819
.word 0x801df5f4,0xa0620818
.word 0x801df8c8,0x90620824
.word 0x801df8d0,0xa0620824
.word 0x801df8dc,0x90a20824
.word 0x801df8e8,0x8ca20814
.word 0x801df940,0x90c20823
.word 0x801df9a8,0x8ca30814
.word 0x801dfa04,0x90620822
.word 0x801dfa10,0x90630821
.word 0x801dfb58,0xa0400822
.word 0x801e0e84,0x90630821
.word 0x801e6ffc,0xa462606a
.word 0x801e7044,0x9445606a
.word 0x801e7634,0xa462606a
.word 0x801e78bc,0xa462606a
.word 0x801e7904,0x9445606a
.word 0x801e7e1c,0xa462606a
.word 0x801e7fb8,0x8c420828
.word 0x801e803c,0x8c420828
.word 0x801e8210,0x8c420828
.word 0x801e8da4,0x8c420828
.word 0x801e8edc,0x8c420828
.word 0x801e953c,0x8c420828
.word 0x801e9740,0x8c420828
.word 0x801ea0fc,0x8c420828
.word 0x801ea4f8,0x8c420828
.word 0x801ea718,0x90430823
.word 0x801ea720,0xa0430823
.word 0x801ead4c,0x90630823
.word 0x801eadb0,0x90620823
.word 0x801eae4c,0x90620823
.word 0x801eaf4c,0x90620823
.word 0x801eafb0,0x90420823
.word 0x801eb034,0x90430823
.word 0x801eb06c,0x90420823
.word 0x801eb0d0,0x90420823
.word 0x801eb154,0x90430823
.word 0x801eb18c,0x90420823
.word 0x801eb1ec,0x90420823
.word 0x801eb270,0x90430823
.word 0x801eb2d0,0x90430823
.word 0x801eb334,0x90420823
.word 0x801eb37c,0xa462606a
.word 0x801eb418,0x90430823
.word 0x801eb44c,0x90420823
.word 0x801eb518,0x90620823
.word 0x801eb5b0,0x90620823
.word 0x801eb80c,0xac620828
.word 0x801eb814,0x90670823
.word 0x801eb98c,0x8c420828
.word 0x801ebb58,0x8c420828
.word 0x801ec034,0x90620818
.word 0x801ec044,0xa0620818
.word 0x801ec080,0x8c420828
.word 0x801ec108,0x8ca20828
.word 0x801ec1a8,0x8c420828
.word 0x801ec1f4,0xa0836088
.word 0x801ec888,0x90430818
.word 0x801ec890,0xa0430818
.word 0x801ec8bc,0x8c420828
.word 0x801eca84,0x90620818
.word 0x801eca8c,0xa0620818
.word 0x801ecb08,0x8c420828
.word 0x801ecbb8,0x90430823
.word 0x801ecbc0,0xa0430823
.word 0x801ed130,0x90630823
.word 0x801ed19c,0x90620823
.word 0x801ed214,0x90620823
.word 0x801ed33c,0x90620823
.word 0x801ed3ac,0x90620823
.word 0x801ed3e8,0x90620823
.word 0x801ed454,0x90620823
.word 0x801ed490,0x90620823
.word 0x801ed4fc,0x90620823
.word 0x801ed534,0x90620823
.word 0x801ed780,0xac620828
.word 0x801ed788,0x90670823
.word 0x801edb50,0x90620818
.word 0x801edb5c,0xa0620818
.word 0x801edb98,0x8c420828
.word 0x801ee53c,0x9042082e
.word 0x801ee550,0x9042082e
.word 0x801ee664,0x9062082e
.word 0x801ee680,0x9062082e
.word 0x801ee688,0xa062082e
.word 0x801ee7a8,0x90620818
.word 0x801ee7b8,0xa0620818
.word 0x801eefdc,0x90426074
.word 0x801ef134,0xa0406074
.word 0x801ef39c,0x90c30818
.word 0x801ef3ac,0xa0c30818
.word 0x801ef3c0,0x90620823
.word 0x801ef3c8,0xa0620823
.word 0x801ef80c,0x90630823
.word 0x801ef864,0x90620823
.word 0x801ef8d8,0x90620823
.word 0x801efa54,0xac620828
.word 0x801efa5c,0x90670823
.word 0x801efae0,0xa0400823
.word 0x801efaec,0xa0400822
.word 0x801efaf8,0xa0400818
.word 0x801efafc,0xa0400819
.word 0x801efb08,0xac400814
.word 0x801efb0c,0xac400828
.word 0x801efb14,0xa0606088
.word 0x801efb28,0xa460606a
.word 0x801efb5c,0x94446076
.word 0x801efba8,0xa4406076
.word 0x801efbc4,0x90426087
.word 0x801efdfc,0x8c430828
.word 0x801efe1c,0xa0406086
.word 0x801efe50,0x90426087
.word 0x801efe68,0xa0626086
.word 0x801efe88,0xa0400823
.word 0x801efe94,0xac400810
.word 0x801efea0,0xa0600822
.balign 8
