#include "game/resource_arena.h"

static void func_00001D50_body(ArenaNode *node);

extern void *resource_alloc(u32 requested);
extern void func_00023460(const void *source, void *destination, u32 bytes);

u8 *resource_realloc(u8 *pointer, u32 requested)
{
    ArenaNode *header;
    ArenaNode *node;
    u8 *replacement;
    s32 index;
    s32 delta;

    if (requested == 0) {
        index = func_00001F9C(pointer);
        if (pointer != 0) {
            header = (ArenaNode *)(pointer - 0x20);
            node = header;
            if (header->available >= 0x21) func_00001D50_body(node);
            header->prev->next = header->next;
            if ((u32)header->next < (u32)D_800AEDB0[index].end)
                header->next->prev = header->prev;
            node = header->prev;
            if (node->available >= 0x21) func_00001D50_body(node);
            node->available = (u32)node->next -
                (u32)((u8 *)node + (node->used + 0x20));
            if (node->available >= 0x21)
                func_00001DE8(&D_800AEDB0[index].root, node);
            header->next = 0;
            header->prev = 0;
            D_800C4818++;
            if (D_800C4818 > 0x3FFFF) {
                func_00001E74();
                D_800C4818 = 0;
            }
        }
        return 0;
    }
    if (pointer == 0) return resource_alloc(requested);

    /* Initialize the real header before rounding. Moving this initialization
     * after the delta calculation makes GCC swap the mask and old-size load. */
    header = (ArenaNode *)(pointer - 0x20);
    requested = (requested + 15) & -16;
    delta = requested - ((ArenaNode *)pointer)[-1].used;
    if (delta < -0x7F ||
        (delta > 0 && header->available >= (u32)delta)) {
        if (header->available >= 0x21) func_00001D50_body(header);
        header->used = requested;
        header->available = (u32)header->next -
            (u32)((u8 *)header + (requested + 0x20));
        if (header->available >= 0x21) {
            index = func_00001F9C(header);
            func_00001DE8(&D_800AEDB0[index].root, header);
        }
    } else if (delta > 0) {
        replacement = resource_alloc(requested);
        /* Retail calls the copy before releasing the old allocation, with
         * the original payload size and no additional allocation-failure test. */
        func_00023460(pointer, replacement, header->used);
        index = func_00001F9C(pointer);
        node = header;
        if (header->available >= 0x21) func_00001D50_body(node);
        header->prev->next = header->next;
        if ((u32)header->next < (u32)D_800AEDB0[index].end)
            header->next->prev = header->prev;
        node = header->prev;
        if (node->available >= 0x21) func_00001D50_body(node);
        node->available = (u32)node->next -
            (u32)((u8 *)node + (node->used + 0x20));
        if (node->available >= 0x21)
            func_00001DE8(&D_800AEDB0[index].root, node);
        header->next = 0;
        header->prev = 0;
        D_800C4818++;
        if (D_800C4818 > 0x3FFFF) {
            func_00001E74();
            D_800C4818 = 0;
        }
        return replacement;
    }
    return pointer;
}

/* LOCAL body retains the fixed-address func_00001D50 entry.
 * Required independent unlink body at owner offset 0x30C. */
static void func_00001D50_body(ArenaNode *node)
{
    ArenaNode *cursor;
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
}
