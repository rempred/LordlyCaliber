/* Soldier actor HP, maximum and status. v0 holds actor.handle*2 on entry.
 * Player layout and the original v0/v1 results remain unchanged. */
.text
.set noat
.set noreorder
.globl lc_capacity_battle_fields
.type lc_capacity_battle_fields, @function
.ent lc_capacity_battle_fields
lc_capacity_battle_fields:
    lui $at, %hi(lc_battle_source)
    lbu $at, %lo(lc_battle_source)($at)
    sltiu $at, $at, 30
    bne $at, $zero, 1f
    nop
    lui $at, %hi(lc_enemy_soldier_hp)
    addu $v0, $at, $v0
    lhu $v0, %lo(lc_enemy_soldier_hp)($v0)
    sh $v0, 32($s1)
    lui $at, %hi(lc_enemy_soldier_max_hp)
    lhu $v1, %lo(lc_enemy_soldier_max_hp)($at)
    sh $v1, 34($s1)
    lbu $v0, 246($s1)
    lui $at, %hi(lc_enemy_soldier_state)
    addu $v0, $at, $v0
    lbu $v0, %lo(lc_enemy_soldier_state)($v0)
    j lc_battle_fields_continue
    nop
1:  addu $v0, $s3, $v0
    lhu $v0, -196($v0)
    sh $v0, 32($s1)
    lbu $v0, 246($s1)
    lhu $v1, 2($s3)
    addu $v0, $s3, $v0
    sh $v1, 34($s1)
    lbu $v0, 144($v0)
    j lc_battle_fields_continue
    nop
.end lc_capacity_battle_fields
.size lc_capacity_battle_fields, . - lc_capacity_battle_fields
