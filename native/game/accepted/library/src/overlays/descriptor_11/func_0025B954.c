#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct B954Entry {
    void *value_00;
    u8 unknown_04[0x2C];
    void *value_30;
    u16 value_34, value_36, value_38, value_3A;
    u16 value_3C, value_3E, value_40, value_42;
} B954Entry;

typedef struct B954State {
    u8 unknown_00[0x58];
    B954Entry **entries_58;
} B954State;

extern void func_00260C24(void *);
extern void *func_00261560(s32);

void func_0025B954(u16 command, B954State *state)
{
    s16 i;

    switch (command) {
    case 1:
        for (i = 5; i < 13; i++) {
            func_00260C24(state->entries_58[i]->value_30);
        }
        break;
    case 0:
        for (i = 5; i < 13; i++) {
            B954Entry *entry = state->entries_58[i];

            entry->value_00 = (void *)func_00264f24;
            entry->value_3E = 0;
            entry->value_3C = 0;
            entry->value_36 = 0;
            entry->value_34 = 0;
            entry->value_42 = 31;
            entry->value_40 = 31;
            entry->value_3A = 31;
            entry->value_38 = 31;
            entry->value_30 = func_00261560(2);
        }
        break;
    case 2:
        for (i = 5; i < 13; i++) {
            B954Entry *entry = state->entries_58[i];
            u32 value36 = entry->value_36;
            u32 value3E = entry->value_3E;

            /* Keep the paired loaded words until the halfword stores. */
            entry->value_36 = value36 + 4;
            entry->value_3A = value36 + 35;
            entry->value_3E = value3E + 6;
            entry->value_42 = value3E + 37;
        }
        break;
    }
}
