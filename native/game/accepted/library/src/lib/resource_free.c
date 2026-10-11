#include "game/resource_arena.h"

s32 resource_free(u8 *pointer)
{
    /* The original validates before testing for a null pointer. */
    s32 index = func_00001F9C(pointer);
    ArenaNode *node;
    ArenaNode *header;
    u8 *used_end;

    if (pointer != 0) {
        header = (ArenaNode *)(pointer - 0x20);
        /* This local is reused for the predecessor after unlinking. */
        node = header;
        if (header->available >= 0x21) {
            func_00001D50(node);
        }
        header->prev->next = header->next;
        if ((u32)header->next < (u32)D_800AEDB0[index].end)
            header->next->prev = header->prev;
        node = header->prev;
        if (node->available >= 0x21) {
            func_00001D50(node);
        }
        /* Pointer arithmetic preserves the retail addition operand order. */
        used_end = (u8 *)node + (node->used + 0x20);
        node->available = (u32)node->next - (u32)used_end;
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
