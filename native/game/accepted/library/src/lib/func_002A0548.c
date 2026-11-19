#include "game/cutscene_dialogue_handlers.h"

typedef struct DialogueTrackingEntry {
    u8 pad00[2];
    s16 windowId;
} DialogueTrackingEntry;

typedef struct DialogueTrackingScene {
    u8 pad000[0x1D8];
    DialogueTrackingEntry *entries[28];
} DialogueTrackingScene;

extern DialogueTrackingScene *D_8022A974;
extern s32 resource_free(u8 *pointer);
/* Window close entry in dialogue overlay 4. */
extern void func_000fa4f4(u8 windowId);

void func_002A0548(s32 operand)
{
    s16 index = 0;

    do {
        DialogueTrackingEntry *entry = D_8022A974->entries[index];

        if (entry != 0 && entry->windowId == operand) {
            resource_free((u8 *)entry);
            D_8022A974->entries[index] = 0;
            break;
        }
        ++index;
    } while (index < 28);

    func_000fa4f4((u8)operand);
}
