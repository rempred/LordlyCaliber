#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct B7b8Entry {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30, value_32, value_34, value_36;
} B7b8Entry;

typedef struct B7b8State {
    u8 unknown_00[0x58];
    B7b8Entry **entries_58;
} B7b8State;

void func_0025B7B8(u16 command, B7b8State *state)
{
    switch (command) {
    case 0: {
        B7b8Entry *entry;

        state->entries_58[4]->value_00 = (void *)func_00263F40;
        entry = state->entries_58[4];
        entry->value_32 = 0;
        entry->value_30 = 0;
        entry->value_36 = 15;
        entry->value_34 = 15;
        break;
    }
    case 2: {
        B7b8Entry *entry = state->entries_58[4];

        entry->value_32 = (entry->value_32 - 2) & 31;
        entry->value_34 = entry->value_30 + 15;
        entry->value_36 = entry->value_32 + 15;
        break;
    }
    case 1:
        break;
    }
}
