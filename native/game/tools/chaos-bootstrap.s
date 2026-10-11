# Maintained source adapted from parent tools/build_cf_army_counter_modregion_rom.py
# and Decomp e50fe500 tools/lordly_phase3_native_payload.py. Assembled at user export.
# Preserve caller RA; dispatch through uncached load-once sentinel.
    lui $t0,0xa042
    ori $t0,$t0,0x0
    lw $t1,0x0($t0)
    lui $t2,0x4f42
    ori $t2,$t2,0x4346
    beq $t1,$t2,loaded
    nop
    j CF_CONTINUATION
    nop
loaded:
    j CF_ENTRY
    nop
.org 0x88
