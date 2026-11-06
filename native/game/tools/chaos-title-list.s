# F3DEX2 title display list. The palette operand is bound to the current
# Army resource before submission; the source default is never rendered.
    .word 0xed000000,0x004fc3bc,0xe7000000,0x00000000
    .word 0xfd100000,0x801fa770,0xe8000000,0x00000000
    .word 0xf5000100,0x07000000,0xe6000000,0x00000000
    .word 0xf0000000,0x0703c000,0xe7000000,0x00000000
    .word 0xfd500000,0x80420700,0xf5500000,0x07000000
    .word 0xe6000000,0x00000000,0xf3000000,0x070bf2ab
    .word 0xe7000000,0x00000000,0xf5400600,0x00000000
    .word 0xf2000000,0x000bc03c,0xe7000000,0x00000000
    .word 0xe4448068,0x003c0048,0xe1000000,0x00000000
    .word 0xf1000000,0x04000400,0xe7000000,0x00000000
    .word 0xe44d8068,0x00448048,0xe1000000,0x00000100
    .word 0xf1000000,0x04000400,0xe7000000,0x00000000
    .word 0xdf000000,0x00000000
.org 0x950
bind_current_artwork:
    # Army owner +4 is its loaded texture pack; +8 is its palette pack.
    # Native War Funds uses texture +0x948 and title palette +0x320.
    # These allocations move when resources/custom class icons change.
    lui $t4,0x8019
    lw $t4,0x6af8($t4)
    lw $t5,0x4($t4)
    addiu $t5,$t5,0x948
    sw $t5,0x234($t0)
    lw $t5,0x8($t4)
    addiu $t5,$t5,0x320
    lui $t4,0xa042
    sw $t5,0x894($t4)
    jr $ra
    nop
.org 0xa00
