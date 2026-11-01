#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct BcdcEntry {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30, value_32, value_34, value_36, value_38;
} BcdcEntry;

typedef struct BcdcState {
    u8 unknown_00[0x58];
    BcdcEntry **entries_58;
} BcdcState;

void func_0025BCDC(u16 command, BcdcState *state)
{
    s16 i;

    switch (command) {
    case 0:
        for (i = 0; i < 2; i++) {
            BcdcEntry *entry;

            state->entries_58[i]->value_00 = (void *)func_00263F40;
            entry = state->entries_58[i];
            entry->value_32 = 0;
            entry->value_30 = 0;
            entry->value_34 = 15;
            entry->value_38 = 0;
            entry->value_36 = entry->value_32 + 31;
        }
        break;
    case 2:
        for (i = 0; i < 2; i++) {
            BcdcEntry *entry = state->entries_58[i];

            entry->value_30 = (entry->value_30 + 2) & 15;
            entry->value_34 = entry->value_30 + 15;
            if ((u32)(entry->value_38 - 3) < 7) {
                entry->value_32 = (entry->value_32 - 2) & 31;
                entry->value_36 = entry->value_32 + 31;
            }
            entry->value_38++;
        }
        break;
    case 1:
        break;
    }
}
