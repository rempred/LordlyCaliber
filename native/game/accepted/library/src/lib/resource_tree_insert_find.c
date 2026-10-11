#include "game/resource_arena.h"

void resource_tree_insert_find(ArenaNode **slot, ArenaNode *node)
{
    ArenaNode *root = *slot;
    if (root == 0) {
        node->tree_slot = slot;
        node->left = 0;
        node->right = 0;
        *slot = node;
    } else {
        if (root->available < node->available) {
            slot = &root->right;
        } else {
            slot = &root->left;
        }
        resource_tree_insert_find(slot, node);
    }
}

/* LOCAL body retains the fixed-address func_00001E3C entry. */
static ArenaNode *func_00001E3C_body(ArenaNode *root, u32 bytes)
{
    ArenaNode *fit = 0;
    while (root != 0) {
        if (root->available < bytes) {
            root = root->right;
        } else {
            fit = root;
            root = fit->left;
        }
    }
    return fit;
}
