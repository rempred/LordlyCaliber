#include "game/resource_arena.h"

extern u32 D_800AEDB4;
extern void func_80093380(void *, s32);

void resource_arena_init(u32 start, u32 size)
{
    /* Separate unaligned and aligned endpoints keep the final AND in the
     * retail position without carrying a mask across the clear call. */
    u32 raw_end = start + size;
    u32 end;
    s32 mask = -16;
    ArenaRecord *arena = D_800AEDB0;
    ArenaNode **root;
    start = (start + 15) & mask;
    end = raw_end & mask;
    arena->base = (ArenaNode *)start;
    func_80093380((void *)start, 0x20);
    D_800AEDB0[0].base->next = (ArenaNode *)end;
    D_800AEDB0[0].base->available = end - start - 0x20;
    /* Retain the root address: a direct field store becomes a longer
     * symbol-relative sequence in this compiler. */
    root = &arena->root;
    *root = 0;
    func_00001DE8(root, arena->base);
    D_800C4818 = 0;
    D_800AEDE0 = 1;
    D_800AEDE2 = 3;
    D_800AEDB4 = (u32)D_800AEDB0[0].base->next;
}
