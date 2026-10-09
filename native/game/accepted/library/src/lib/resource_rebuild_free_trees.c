#include "game/resource_arena.h"

void resource_rebuild_free_trees(void)
{
    u16 mode = D_800AEDE2;
    s32 index = (mode >> 1) & 1;
    s32 iterations;
    ArenaNode **root_cursor;
    ArenaNode **end_cursor;
    ArenaNode *node;
    ArenaNode *root;
    ArenaNode **slot;
    ArenaNode **first_root;
    s32 table_offset;

    if (index != 0) {
        iterations = D_800AEDE0 - ((~mode) & 1);
    } else {
        iterations = mode & 1;
    }
    index = 0;
    if (index < iterations) {
        first_root = &D_800AEDB0[0].root;
        end_cursor = first_root - 1;
        /* Keep the byte stride distinct from the iteration count. Letting
         * GCC derive it from the index moves its initialization past the
         * root-cursor assignment and swaps two retail instructions. */
        table_offset = 0;
        root_cursor = first_root;
        do {
            node = ((ArenaRecord *)((u8 *)D_800AEDB0 + table_offset))->base;
            ((ArenaRecord *)((u8 *)D_800AEDB0 + table_offset))->root = 0;
            while ((u32)node < (u32)*end_cursor) {
                if (node->available >= 0x21) {
                    root = *root_cursor;
                    if (root == 0) {
                        node->tree_slot = root_cursor;
                        node->left = 0;
                        node->right = 0;
                        *root_cursor = node;
                    } else {
                        if (root->available >= node->available) {
                            slot = &root->left;
                        } else {
                            slot = &root->right;
                        }
                        func_00001DE8(slot, node);
                    }
                }
                node = node->next;
            }
            end_cursor += 3;
            table_offset += sizeof(ArenaRecord);
            ++index;
            root_cursor += 3;
        } while (index < iterations);
    }
}
