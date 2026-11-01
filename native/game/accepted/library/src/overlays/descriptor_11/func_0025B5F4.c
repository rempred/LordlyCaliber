#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct B5f4Entry {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30, value_32, value_34, value_36;
} B5f4Entry;

typedef struct B5f4State {
    u8 unknown_00[0x58];
    B5f4Entry **entries_58;
} B5f4State;

/* Address only; this source makes no call to the provider. */
void func_0025B5F4(u16 command, B5f4State *state)
{
    s16 i;

    switch (command) {
    case 0:
        for (i = 1; i < 3; i++) {
            B5f4Entry *entry;

            state->entries_58[i]->value_00 = (void *)func_00263F40;
            entry = state->entries_58[i];
            entry->value_32 = 0;
            entry->value_30 = 0;
            entry->value_34 = 15;
            entry->value_36 = entry->value_32 + 31;
        }
        break;
    case 2:
        for (i = 1; i < 3; i++) {
            B5f4Entry *entry = state->entries_58[i];

            entry->value_30 += 2;
            entry->value_32++;
            entry->value_30 &= 15;
            entry->value_32 &= 31;
            entry->value_34 = entry->value_30 + 15;
            entry->value_36 = entry->value_32 + 31;
        }
        break;
    case 1:
        break;
    }
}
