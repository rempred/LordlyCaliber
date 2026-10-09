#include "game/resource_arena.h"

u32 resource_largest_free_block(void)
{
    u16 mode = D_800AEDE2;
    s32 index = (mode >> 1) & 1;
    s32 iterations;
    /* Keep the scan divisor separate from the first selection count. */
    s32 count;
    s32 n;
    s32 scanned;
    u32 largest = 0;
    ArenaNode *node;
    u32 available;

    if (index != 0) {
        n = D_800AEDE0;
        iterations = n - ((~mode) & 1);
        index &= -((u32)n >= 2);
    } else {
        iterations = mode & 1;
    }
    /* Keeping this initialization next to the real loop comparison retains
     * the compiler's eight-byte frame and load-before-prologue prefix. */
    scanned = 0;
    if (scanned < iterations) {
        count = D_800AEDE0;
        do {
            node = D_800AEDB0[index].root;
            while (node != 0) {
                available = node->available;
                if (largest < available) largest = available;
                node = node->right;
            }
            ++scanned;
            index = (index + 1) % count;
        } while (scanned < iterations);
    }
    return largest;
}
