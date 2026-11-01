#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct B714Entry {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30, value_32, value_34, value_36;
} B714Entry;

typedef struct B714State {
    u8 unknown_00[0x58];
    B714Entry **entries_58;
} B714State;

void func_0025B714(u16 command, B714State *state)
{
    switch (command) {
    case 0: {
        B714Entry *entry;

        state->entries_58[0]->value_00 = (void *)func_00263F40;
        entry = state->entries_58[0];
        entry->value_32 = 0;
        entry->value_30 = 0;
        entry->value_36 = 15;
        entry->value_34 = 15;
        break;
    }
    case 2: {
        B714Entry *entry = state->entries_58[0];

        entry->value_32 = (entry->value_32 - 2) & 31;
        entry->value_34 = entry->value_30 + 15;
        entry->value_36 = entry->value_32 + 15;
        break;
    }
    case 1:
        break;
    }
}
