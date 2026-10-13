/* Replace the three-instruction member-kind decision on either combat side.
 * Only the assembler scratch register and the original v0 result are clobbered.
 * In particular v1 already points to the ordinary record and must survive.
 */
.text
.set noat
.set noreorder
.globl lc_capacity_battle_type
.type lc_capacity_battle_type, @function
.ent lc_capacity_battle_type
lc_capacity_battle_type:
    lui $at, %hi(lc_battle_source)
    lbu $at, %lo(lc_battle_source)($at)
    sltiu $at, $at, 30
    bne $at, $zero, 1f
    nop
    lui $at, %hi(lc_enemy_types)
    addu $at, $at, $s2
    lbu $at, %lo(lc_enemy_types)($at)
    addiu $at, $at, -2
    bne $at, $zero, 2f
    sll $v0, $s2, 1
    j lc_battle_soldier
    nop
1:  sltiu $at, $s2, 100
    beq $at, $zero, 3f
    sll $v0, $s2, 1
2:  j lc_battle_ordinary
    nop
3:  j lc_battle_soldier
    nop
.end lc_capacity_battle_type
.size lc_capacity_battle_type, . - lc_capacity_battle_type
