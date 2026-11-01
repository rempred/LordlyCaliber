#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct Bef8Entry {
    void *value_00;
    u8 unknown_04[0x2C];
    u16 value_30, value_32, value_34, value_36, value_38;
} Bef8Entry;

typedef struct Bef8State {
    u8 unknown_00[0x58];
    Bef8Entry **entries_58;
} Bef8State;

extern float sinf(float);
extern float func_00020bf0(float);
extern const double D_8020A2A0, D_8020A2A8;

void func_0025BEF8(u16 command, Bef8State *state)
{
    s16 i;

    switch (command) {
    case 0:
        for (i = 1; i < 3; i++) {
            Bef8Entry *entry;

            state->entries_58[i]->value_00 = (void *)func_00263F40;
            entry = state->entries_58[i];
            entry->value_32 = 0;
            entry->value_30 = 0;
            entry->value_36 = 15;
            entry->value_34 = 15;
            entry->value_38 = 0;
        }
        break;
    case 2:
        for (i = 1; i < 3; i++) {
            Bef8Entry *entry = state->entries_58[i];

            if ((s16)entry->value_38 < 46) {
                entry->value_30 = (s32)(sinf((float)(
                    (double)((s16)entry->value_38 * 8) /
                    D_8020A2A0 * D_8020A2A8)) * 8.0f);
                entry->value_32 = (s32)(func_00020bf0((float)(
                    (double)((s16)entry->value_38 * 8) /
                    D_8020A2A0 * D_8020A2A8)) * 8.0f);
                entry->value_30 &= 15;
                entry->value_32 &= 15;
                entry->value_38++;
                entry->value_34 = entry->value_30 + 15;
                entry->value_36 = entry->value_32 + 15;
            }
        }
        break;
    case 1:
        break;
    }
}
