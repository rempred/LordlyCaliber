#include "game/resource_arena.h"

static ArenaNode *func_00002274(ArenaNode *root, u32 bytes);

void *resource_alloc_tree_scan(u32 requested)
{
    u16 mode;
    s32 index;
    s32 iterations;
    s32 count;
    s32 scanned;
    ArenaNode *node;
    ArenaNode *allocated;
    ArenaNode *cursor;
    ArenaNode *root;
    ArenaNode **root_slot;
    ArenaNode **slot;

    /* Retain the aligned size in the parameter's live range. A separate
     * aligned local swaps this value and the iteration count's saved
     * registers in the pinned compiler. */
    requested = (requested + 15) & -16;
    if (requested == 0) return 0;
    mode = D_800AEDE2;
    index = (mode >> 1) & 1;
    if (index != 0) {
        count = D_800AEDE0;
        iterations = count - ((~mode) & 1);
        index &= -((u32)count >= 2);
    } else {
        iterations = mode & 1;
    }
    scanned = 0;
    if (scanned < iterations) {
        do {
            node = func_00002274(D_800AEDB0[index].root, requested + 0x20);
            if (node != 0) {
                allocated = (ArenaNode *)((u8 *)node + node->used + node->available - requested);
                allocated->next = node->next;
                node->next = allocated;
                if ((u32)allocated->next < (u32)D_800AEDB0[index].end)
                    allocated->next->prev = allocated;
                allocated->prev = node;

                if (node->left == 0) {
                    if (node->right != 0) {
                        *node->tree_slot = node->right;
                        node->right->tree_slot = node->tree_slot;
                    } else {
                        *node->tree_slot = 0;
                    }
                } else {
                    *node->tree_slot = node->left;
                    node->left->tree_slot = node->tree_slot;
                    if (node->right != 0) {
                        cursor = node->left;
                        while (cursor->right != 0) {
                            cursor = cursor->right;
                        }
                        cursor->right = node->right;
                        node->right->tree_slot = &cursor->right;
                    }
                }

                node->available = node->available - requested - 0x20;
                root_slot = &D_800AEDB0[index].root;
                allocated->used = requested;
                allocated->available = 0;
                allocated->tree_slot = root_slot;
                allocated->left = 0;
                allocated->right = 0;
                if (node->available >= 0x21) {
                    root = *root_slot;
                    if (root == 0) {
                        node->tree_slot = root_slot;
                        node->left = 0;
                        node->right = 0;
                        *root_slot = node;
                    } else {
                        if (root->available >= node->available) {
                            slot = &root->left;
                        } else {
                            slot = &root->right;
                        }
                        func_00001DE8(slot, node);
                    }
                }
                return (u8 *)allocated + 0x20;
            }
            /* GCC puts the real miss-path update in the null branch's slot. */
            ++scanned;
            index = (index + 1) % D_800AEDE0;
        } while (scanned < iterations);
    }
    return 0;
}

/* Required callable traversal body at owner offset 0x270. */
static ArenaNode *func_00002274(ArenaNode *root, u32 bytes)
{
    if (root == 0) return 0;
    while (root->right != 0) {
        root = root->right;
    }
    return (ArenaNode *)((u32)root & -(root->available >= bytes));
}
