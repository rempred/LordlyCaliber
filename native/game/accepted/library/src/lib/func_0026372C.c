#include "common/types.h"

typedef struct ResourceEntry16 {
    s16 unknown00;
    u16 code02;
    s32 key04;
    void *data08;
    u16 next0C;
    u16 unknown0E;
} ResourceEntry16;

extern ResourceEntry16 *D_80220E14;
extern u16 D_80220E10;
extern void func_00260C24(void *data);

void func_0026372C(void)
{
    ResourceEntry16 *entry;

    for (entry = D_80220E14; entry < D_80220E14 + 64; entry++) {
        if (entry->unknown00 != 0 && --entry->unknown00 == 0) {
            func_00260C24(entry->data08);
            /* This store order preserves the retail field-relative addressing
             * in KMC; placing the link store after the clears changes it. */
            entry->next0C = D_80220E10;
            entry->code02 = 0;
            entry->key04 = 0;
            entry->data08 = 0;
            D_80220E10 = entry - D_80220E14;
        }
    }
}