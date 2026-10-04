/* Register adapters for the original encounter callers. Logic lives in custom.c.
 * Each entry is linked with the core; no fixed internal entry offsets. */
.text
.set noat
.set noreorder

.globl lc_selection_site
.type lc_selection_site, @function
.ent lc_selection_site
lc_selection_site:
    addiu $sp, $sp, -24
    sw $ra, 20($sp)
    move $a0, $v0
    move $a1, $s0
    jal lc_custom_select
    move $a2, $v1
    lw $ra, 20($sp)
    addiu $sp, $sp, 24
    beq $v0, $zero, 1f
    nop
    j lc_selection_success
    addiu $v0, $v0, 256
1:  jr $ra
    addiu $v0, $v0, 256
.end lc_selection_site
.size lc_selection_site, . - lc_selection_site

.globl lc_materializer_site
.type lc_materializer_site, @function
.ent lc_materializer_site
lc_materializer_site:
    addiu $sp, $sp, -24
    move $a0, $s1
    jal lc_custom_materialize
    move $a1, $s2
    beq $v0, $zero, 1f
    addiu $sp, $sp, 24
    j lc_materializer_epilogue
    nop
1:  addiu $v0, $zero, 1
    bne $s3, $v0, 2f
    move $a0, $zero
    j lc_materializer_class_one
    nop
2:  j lc_materializer_generic
    nop
.end lc_materializer_site
.size lc_materializer_site, . - lc_materializer_site

.globl lc_card_message_site
.type lc_card_message_site, @function
.ent lc_card_message_site
lc_card_message_site:
    lui $t0, %hi(lc_custom_state)
    lw $t0, %lo(lc_custom_state + 4)($t0)
    beq $t0, $zero, 1f
    nop
    jr $ra
    addiu $a1, $zero, 15
1:  addu $a1, $a1, $v0
    jr $ra
    andi $a1, $a1, 65535
.end lc_card_message_site
.size lc_card_message_site, . - lc_card_message_site

.globl lc_card_text_site
.type lc_card_text_site, @function
.ent lc_card_text_site
lc_card_text_site:
    lui $t0, %hi(lc_message_index)
    addiu $t0, $t0, %lo(lc_message_index)
    lbu $t1, 0($t0)
    andi $t1, $t1, 63
    bne $t1, $zero, 1f
    nop
    lbu $t1, 1($t0)
    addiu $t2, $zero, 15
    bne $t1, $t2, 1f
    nop
    lui $t0, %hi(lc_custom_state)
    lw $t1, %lo(lc_custom_state + 4)($t0)
    beq $t1, $zero, 1f
    nop
    lui $v1, %hi(lc_custom_state + 84)
    addiu $v1, $v1, %lo(lc_custom_state + 84)
    jr $ra
    sw $v1, 4($a0)
1:  addu $v1, $s0, $v1
    jr $ra
    sw $v1, 4($a0)
.end lc_card_text_site
.size lc_card_text_site, . - lc_card_text_site

.globl lc_persuasion_site
.type lc_persuasion_site, @function
.ent lc_persuasion_site
lc_persuasion_site:
    addiu $sp, $sp, -32
    sw $ra, 28($sp)
    sw $a0, 24($sp)
    move $a0, $s0
    move $a1, $s1
    jal lc_custom_persuade
    move $a2, $s2
    bltz $v0, 1f
    nop
    j lc_persuasion_epilogue
    addiu $sp, $sp, 32
1:  lw $a0, 24($sp)
    jal lc_persuasion_helper
    lw $a1, 0x4c($s0)
    lw $ra, 28($sp)
    jr $ra
    addiu $sp, $sp, 32
.end lc_persuasion_site
.size lc_persuasion_site, . - lc_persuasion_site

.globl lc_talk_target_site
.type lc_talk_target_site, @function
.ent lc_talk_target_site
lc_talk_target_site:
    lui $t0, %hi(lc_custom_state)
    addiu $t0, $t0, %lo(lc_custom_state)
    lw $t1, 4($t0)
    beq $t1, $zero, 1f
    nop
    lw $v0, 12($t0)
    beq $v0, $zero, 1f
    nop
    jr $ra
    nop
1:  j lc_persuasion_target
    nop
.end lc_talk_target_site
.size lc_talk_target_site, . - lc_talk_target_site

.globl lc_retreat_site
.type lc_retreat_site, @function
.ent lc_retreat_site
lc_retreat_site:
    addiu $sp, $sp, -24
    sw $ra, 20($sp)
    jal lc_custom_retreat
    move $a0, $s0
    lw $ra, 20($sp)
    bltz $v0, 1f
    addiu $sp, $sp, 24
    j lc_retreat_epilogue
    nop
1:  move $a0, $zero
    jr $ra
    lui $a1, 0x31
.end lc_retreat_site
.size lc_retreat_site, . - lc_retreat_site

.globl lc_fight_budget_site
.type lc_fight_budget_site, @function
.ent lc_fight_budget_site
lc_fight_budget_site:
    lui $t0, %hi(lc_custom_state)
    lw $t0, %lo(lc_custom_state + 4)($t0)
    beq $t0, $zero, 1f
    nop
    jr $ra
    addiu $v0, $zero, 2
1:  jr $ra
    addiu $v0, $v0, -1
.end lc_fight_budget_site
.size lc_fight_budget_site, . - lc_fight_budget_site

.globl lc_talk_budget_site
.type lc_talk_budget_site, @function
.ent lc_talk_budget_site
lc_talk_budget_site:
    lui $t0, %hi(lc_custom_state)
    lw $t0, %lo(lc_custom_state + 4)($t0)
    beq $t0, $zero, 1f
    nop
    jr $ra
    addiu $v0, $zero, 2
1:  jr $ra
    addiu $v0, $v0, -2
.end lc_talk_budget_site
.size lc_talk_budget_site, . - lc_talk_budget_site

.globl lc_round_site
.type lc_round_site, @function
.ent lc_round_site
lc_round_site:
    addiu $sp, $sp, -24
    sw $ra, 20($sp)
    jal lc_round_mode
    nop
    bne $v0, $zero, 1f
    nop
    lui $t0, %hi(lc_custom_state)
    lw $v0, %lo(lc_custom_state + 4)($t0)
    sltu $v0, $zero, $v0
1:  lw $ra, 20($sp)
    jr $ra
    addiu $sp, $sp, 24
.end lc_round_site
.size lc_round_site, . - lc_round_site

/* Preserve an advancing/end result. Skip only zero-width menu padding inside
 * the stream's declared size when the original iterator makes no progress. */
.globl lc_menu_iterator_site
.type lc_menu_iterator_site, @function
.ent lc_menu_iterator_site
lc_menu_iterator_site:
    addiu $sp, $sp, -24
    sw $ra, 20($sp)
    jal lc_menu_iterator
    sw $a0, 16($sp)
    lw $t0, 16($sp)
    bne $v0, $t0, 3f
    nop
    lui $t1, %hi(lc_menu_context)
    lw $t1, %lo(lc_menu_context)($t1)
    beq $t1, $zero, 2f
    nop
    lw $t2, 0x814($t1)
    addiu $v0, $t0, 1
1:  sltu $t3, $v0, $t2
    beq $t3, $zero, 2f
    nop
    addu $t4, $t1, $v0
    lbu $t5, 0x10($t4)
    beq $t5, $zero, 4f
    addiu $t6, $zero, 255
    bne $t5, $t6, 3f
    nop
4:  beq $zero, $zero, 1b
    addiu $v0, $v0, 1
2:  addiu $v0, $zero, -1
3:  lw $ra, 20($sp)
    jr $ra
    addiu $sp, $sp, 24
.end lc_menu_iterator_site
.size lc_menu_iterator_site, . - lc_menu_iterator_site
.set reorder
.set at
