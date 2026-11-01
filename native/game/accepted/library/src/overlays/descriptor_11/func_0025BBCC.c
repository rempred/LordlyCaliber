#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct BbccEntry {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30, value_32, value_34, value_36, value_38;
} BbccEntry;

typedef struct BbccState {
    u8 unknown_00[0x58];
    BbccEntry **entries_58;
} BbccState;

void func_0025BBCC(u16 command, BbccState *state)
{
    s16 i;

    switch (command) {
    case 0:
        for (i = 2; i < 10; i++) {
            BbccEntry *entry;

            state->entries_58[i]->value_00 = (void *)func_00263F40;
            entry = state->entries_58[i];
            entry->value_32 = 0;
            entry->value_30 = 0;
            entry->value_36 = 31;
            entry->value_34 = 31;
            entry->value_38 = 0;
        }
        break;
    case 2:
        for (i = 2; i < 10; i++) {
            BbccEntry *entry = state->entries_58[i];

            if ((u32)(entry->value_38++ - 24) < 14) {
                entry->value_30 = (entry->value_30 + 1) & 31;
                entry->value_34 = entry->value_30 + 31;
                entry->value_36 = entry->value_32 + 31;
            }
        }
        break;
    case 1:
        break;
    }
}
