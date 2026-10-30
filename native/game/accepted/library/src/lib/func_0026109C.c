#include "common/types.h"

typedef struct ResourceRecord10 {
    u16 unknown00;
    u16 flags02;
    u16 unknown04;
    u16 unknown06;
    u16 unknown08;
} ResourceRecord10;

typedef struct ResourceBlock {
    u32 unknown00;
    u32 count04;
    u32 **tables08;
    ResourceRecord10 *records0C;
    void *data10;
    u32 *offsets14;
    u32 *offsets18;
} ResourceBlock;

void func_0026109C(ResourceBlock *block)
{
    u32 i;
    u32 j;
    u32 *entries;
    u32 *record;
    u16 flags;

    block->tables08 = (u32 **)((u8 *)block->tables08 + (u32)block);
    block->records0C = (ResourceRecord10 *)((u8 *)block->records0C + (u32)block);
    block->data10 = (u8 *)block->data10 + (u32)block;
    if (block->offsets14 != 0) {
        block->offsets14 = (u32 *)((u8 *)block->offsets14 + (u32)block);
    }
    block->offsets18 = (u32 *)((u8 *)block->offsets18 + (u32)block);

    for (i = 0; i < block->count04; i++) {
        if (block->tables08[i] != 0) {
            block->tables08[i] = (u32 *)((u8 *)block->tables08[i] + (u32)block);
            entries = block->tables08[i];
            j = 0;
            do {
                j++;
                if (*entries != 0) {
                    record = (u32 *)(*entries + (u32)block);
                    *entries = (u32)record;
                    *record += (u32)block;
                }
                entries++;
            } while (j < 10);
        }
    }
    for (i = 0; i < block->count04; i++) {
        flags = block->records0C[i].flags02;
        if (flags & 0x8000) {
            j = flags & 0x7FFF;
            block->offsets14[j] += (u32)block;
        }
    }
    for (i = 0; i < block->count04; i++) {
        if (block->offsets18[i] != 0) {
            block->offsets18[i] += (u32)block;
        }
    }
}
