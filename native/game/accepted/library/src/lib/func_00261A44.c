#include "game/effect_model_e06.h"
#include "common/types.h"

typedef struct ResourceSlot {
    u16 unknown00;
    u16 code02;
    s32 key04;
    s32 key08;
    void *data0C;
    u16 next;
    u16 unknown12;
} ResourceSlot;

extern void *func_00260B0C(u16 code, s32 key04, s32 key08);
extern void func_00260B44(void *data);
extern ResourceSlot *func_00260B98(void);
extern void *func_00260F30(u16 code, s32 index);


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

/* Literal-code helpers avoid a code parameter kept live across the calls. */
static inline void *load_resource_5(s32 key04)
{
    void *data = 0;
    ResourceSlot *slot;

    if (key04 > 0) {
        data = func_00260B0C(5, key04, 0);
        if (data != 0) {
            func_00260B44(data);
            return data;
        }
        slot = func_00260B98();
        if (slot != 0) {
            data = func_00260F30(5, key04);
            if (data != 0) {
                slot->unknown00 = 1;
                slot->data0C = data;
                slot->code02 = 5;
                slot->key04 = key04;
                slot->key08 = 0;
            }
        }
    }
    return data;
}


static inline void *load_resource_6(s32 key04)
{
    void *data = 0;
    ResourceSlot *slot;

    if (key04 > 0) {
        data = func_00260B0C(6, key04, 0);
        if (data != 0) {
            func_00260B44(data);
            return data;
        }
        slot = func_00260B98();
        if (slot != 0) {
            data = func_00260F30(6, key04);
            if (data != 0) {
                slot->unknown00 = 1;
                slot->data0C = data;
                slot->code02 = 6;
                slot->key04 = key04;
                slot->key08 = 0;
            }
        }
    }
    return data;
}


extern void *func_00261608(u16 code, s32 key04);

s32 func_00261A44(ResourceBlock *block, u16 resourceCode, s32 offset, u16 unused0C, s32 unused10)
{
    s32 foundFlag = 0;
    ResourceRecord10 *record = block->records0C;
    s32 remaining = block->count04;
    u16 flags;

    for (; remaining > 0; remaining--, record++) {
        if (record->unknown06 != 0) {
            load_resource_6(record->unknown06);
            load_resource_5(record->unknown00);
            flags = record->flags02;
            if (flags & 0x8000) {
                foundFlag = 1;
            } else {
                func_00261608(resourceCode, flags + offset);
            }
        }
    }
    return foundFlag;
}