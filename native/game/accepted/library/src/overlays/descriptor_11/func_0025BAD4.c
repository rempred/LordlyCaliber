#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct Bad4Entry {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30, value_32, value_34, value_36;
} Bad4Entry;

typedef struct Bad4State {
    u8 unknown_00[0x58];
    Bad4Entry **entries_58;
} Bad4State;

/* Address only; this source makes no call to the provider. */
void func_0025BAD4(u16 command, Bad4State *state)
{
    s16 i;

    switch (command) {
    case 0:
        for (i = 0; i < 2; i++) {
            Bad4Entry *entry;

            state->entries_58[i]->value_00 = (void *)func_00263F40;
            entry = state->entries_58[i];
            entry->value_32 = 0;
            entry->value_30 = 0;
            entry->value_36 = 15;
            entry->value_34 = 15;
        }
        break;
    case 2:
        for (i = 0; i < 2; i++) {
            Bad4Entry *entry = state->entries_58[i];

            entry->value_32 = (entry->value_32 + 2) & 31;
            entry->value_34 = entry->value_30 + 15;
            entry->value_36 = entry->value_32 + 15;
        }
        break;
    case 1:
        break;
    }
}
