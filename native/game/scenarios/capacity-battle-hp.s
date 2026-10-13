/* Initial Soldier HP eligibility read. v0 holds handle*2 on entry. */
.text
.set noat
.set noreorder
.globl lc_capacity_battle_hp
.type lc_capacity_battle_hp, @function
.ent lc_capacity_battle_hp
lc_capacity_battle_hp:
    lui $at, %hi(lc_battle_source)
    lbu $at, %lo(lc_battle_source)($at)
    sltiu $at, $at, 30
    bne $at, $zero, 1f
    nop
    lui $at, %hi(lc_enemy_soldier_hp)
    addu $v0, $at, $v0
    lhu $v0, %lo(lc_enemy_soldier_hp)($v0)
    j lc_battle_hp_continue
    nop
1:  addu $v0, $s3, $v0
    lhu $v0, -196($v0)
    j lc_battle_hp_continue
    nop
.end lc_capacity_battle_hp
.size lc_capacity_battle_hp, . - lc_capacity_battle_hp
