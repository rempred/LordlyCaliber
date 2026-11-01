#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct C1e8Entry {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30, value_32, value_34, value_36;
} C1e8Entry;

typedef struct C1e8State {
    u8 unknown_00[0x58];
    C1e8Entry **entries_58;
} C1e8State;

/* Address only; this source makes no call to the provider. */
void func_0025C1E8(u16 command, C1e8State *state)
{
    s16 i;

    switch (command) {
    case 0:
        for (i = 0; i < 8; i++) {
            C1e8Entry *entry;

            state->entries_58[i]->value_00 = (void *)func_00263F40;
            entry = state->entries_58[i];
            entry->value_32 = 0;
            entry->value_30 = 0;
            entry->value_34 = 7;
            entry->value_36 = entry->value_32 + 31;
        }
        break;
    case 2:
        for (i = 0; i < 8; i++) {
            C1e8Entry *entry = state->entries_58[i];

            entry->value_32 = (entry->value_32 + 2) & 31;
            entry->value_34 = entry->value_30 + 7;
            entry->value_36 = entry->value_32 + 31;
        }
        break;
    case 1:
        break;
    }
}
