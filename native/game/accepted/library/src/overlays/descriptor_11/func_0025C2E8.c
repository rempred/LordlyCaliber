#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct C2e8Entry {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30, value_32, value_34, value_36;
} C2e8Entry;

typedef struct C2e8State {
    u8 unknown_00[0x58];
    C2e8Entry **entries_58;
} C2e8State;

/* Address only; this source makes no call to the provider. */
void func_0025C2E8(u16 command, C2e8State *state)
{
    s16 i;

    switch (command) {
    case 0:
        for (i = 2; i < 4; i++) {
            C2e8Entry *entry;

            state->entries_58[i]->value_00 = (void *)func_00263F40;
            entry = state->entries_58[i];
            entry->value_32 = 0;
            entry->value_30 = 0;
            entry->value_36 = 31;
            entry->value_34 = 31;
        }
        break;
    case 2:
        for (i = 2; i < 4; i++) {
            C2e8Entry *entry = state->entries_58[i];

            entry->value_30 = (entry->value_30 + 1) & 31;
            entry->value_34 = entry->value_30 + 31;
            entry->value_36 = entry->value_32 + 31;
        }
        break;
    case 1:
        break;
    }
}
