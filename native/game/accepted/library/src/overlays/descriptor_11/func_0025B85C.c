#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct B85cEntry {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30, value_32, value_34, value_36;
} B85cEntry;

typedef struct B85cState {
    u8 unknown_00[0x58];
    B85cEntry **entries_58;
} B85cState;

/* Address only; this source makes no call to the provider. */
void func_0025B85C(u16 command, B85cState *state)
{
    s16 i;

    switch (command) {
    case 0:
        for (i = 2; i < 16; i++) {
            B85cEntry *entry;

            state->entries_58[i]->value_00 = (void *)func_00263F40;
            entry = state->entries_58[i];
            entry->value_32 = 0;
            entry->value_30 = 0;
            entry->value_36 = 15;
            entry->value_34 = 15;
        }
        break;
    case 2:
        for (i = 2; i < 16; i++) {
            B85cEntry *entry = state->entries_58[i];

            entry->value_32 = (entry->value_32 - 2) & 31;
            entry->value_34 = entry->value_30 + 15;
            entry->value_36 = entry->value_32 + 15;
        }
        break;
    case 1:
        break;
    }
}
