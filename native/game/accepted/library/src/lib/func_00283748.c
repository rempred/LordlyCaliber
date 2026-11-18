#include "game/cutscene_panels.h"

extern s32 resource_free(u8 *);

void func_00283748(s32 slot)
{
    u8 index = 0;
    u8 end = 14;
    u32 current;

    if (slot != -1) {
        index = slot;
        end = slot + 1;
    }

    current = (u8)index;
    if (current < end) {
        /* Preserve the narrow comparison, then carry the word bound through the loop. */
        u32 last = end;
        do {
            PanelSlot *record = D_8022A974->slots[current];
            ++index;
            if (record != 0) {
                resource_free(record->field10);
                resource_free(record->field08);
                resource_free(record->field04);
                resource_free((u8 *)record);
                D_8022A974->slots[current] = 0;
            }
            current = (u8)index;
        } while (current < last);
    }
}
