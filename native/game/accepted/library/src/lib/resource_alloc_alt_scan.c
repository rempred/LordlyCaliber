#include "game/resource_arena.h"

void *resource_alloc_alt_scan(u32 requested)
{
    u32 aligned = (requested + 15) & -16;
    u16 mode;
    s32 index;
    s32 iterations;
    s32 n;
    s32 scanned;
    u32 needed;
    ArenaNode *node;
    u8 *anchor;
    ArenaNode *allocated;

    if (aligned == 0) return 0;
    mode = D_800AEDE2;
    index = (mode >> 1) & 1;
    if (index != 0) {
        n = D_800AEDE0;
        iterations = n - ((~mode) & 1);
        index &= -((u32)n >= 2);
    } else {
        iterations = mode & 1;
    }
    scanned = 0;
    if (scanned < iterations) {
        needed = aligned + 0x20;
        do {
            node = func_00001E3C(D_800AEDB0[index].root, needed);
            if (node != 0) {
                anchor = (u8 *)node + node->used;
                allocated = (ArenaNode *)(anchor + 0x20);
                allocated->next = node->next;
                node->next = allocated;
                if ((u32)allocated->next < (u32)D_800AEDB0[index].end)
                    allocated->next->prev = allocated;
                allocated->prev = node;
                func_00001D50(node);
                node->available = 0;
                allocated->used = aligned;
                allocated->available = (u32)allocated->next - (u32)allocated - needed;
                if (allocated->available >= 0x21)
                    func_00001DE8(&D_800AEDB0[index].root, allocated);
                return anchor + 0x40;
            }
            /* Keep this update on the miss path in C: GCC schedules it into
             * the node-null branch delay slot. */
            ++scanned;
            index = (index + 1) % D_800AEDE0;
        } while (scanned < iterations);
    }
    return 0;
}
