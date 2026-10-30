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

extern void func_00260CE0(u16 code, s32 key04, s32 key08);

void func_00261BD4(ResourceBlock *block, u16 resourceCode, s32 offset)
{
    ResourceRecord10 *record = block->records0C;
    s32 remaining = block->count04;
    u16 code;

    for (; remaining > 0; remaining--, record++) {
        if (record->unknown06 != 0) {
            func_00260CE0(6, record->unknown06, 0);
            if (record->unknown00 != 0) {
                func_00260CE0(5, record->unknown00, 0);
            }
            code = record->flags02;
            if (code & 0x8000) {
                continue;
            }
            func_00260CE0(resourceCode, code + offset, 0);
        }
    }
}