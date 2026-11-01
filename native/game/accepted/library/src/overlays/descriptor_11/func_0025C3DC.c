#include "game/effect_geometry_e05.h"
#include "game/effect_adapters_e08.h"

typedef struct C3dcEntry {
    void *value_00;
    u8 unknown_04[0x2C];
    void *value_30;
    u16 value_34, value_36, value_38, value_3A;
    u16 value_3C, value_3E, value_40, value_42;
} C3dcEntry;

typedef struct C3dcState {
    u8 unknown_00[0x58];
    C3dcEntry **entries_58;
} C3dcState;

extern s16 D_8020A1AC[];
extern void func_00260C24(void *);
extern void *func_00261560(s32);

void func_0025C3DC(u16 command, C3dcState *state)
{
    /* Separate cursors retain each walk's original allocation and lifetime. */
    s16 *index0;
    s16 *index1;
    s16 *index2;
    C3dcEntry *entry0;
    C3dcEntry *entry2;
    u32 value3E;
    u32 value31;
    void *value00;

    switch (command) {
    case 0: {
        if (D_8020A1AC[0] >= 0) {
            /* Keep the common value and code address ahead of the cursor. */
            value31 = 31;
            value00 = (void *)func_00264f24;
            index0 = D_8020A1AC;
            do {
                entry0 = state->entries_58[*index0];

                entry0->value_00 = value00;
                entry0->value_36 = 0;
                entry0->value_34 = 0;
                entry0->value_3A = value31;
                entry0->value_38 = value31;
                entry0->value_3E = 0;
                entry0->value_3C = 0;
                entry0->value_42 = value31;
                entry0->value_40 = value31;
                entry0->value_30 = func_00261560(1);
                index0++;
            } while (*index0 >= 0);
        }
        break;
    }
    case 1: {
        if (D_8020A1AC[0] >= 0) {
            index1 = D_8020A1AC;
            do {
                func_00260C24(state->entries_58[*index1]->value_30);
                index1++;
            } while (*index1 >= 0);
        }
        break;
    }
    case 2: {
        if (D_8020A1AC[0] >= 0) {
            index2 = D_8020A1AC;
            do {
                entry2 = state->entries_58[*index2];

                /* The original performs this halfword load/store unchanged. */
                entry2->value_36 = entry2->value_36;
                value3E = entry2->value_3E;
                entry2->value_34 += 2;
                entry2->value_3E = value3E + 2;
                entry2->value_42 = value3E + 33;
                entry2->value_38 = entry2->value_34 + 31;
                entry2->value_3A = entry2->value_36 + 31;
                index2++;
            } while (*index2 >= 0);
        }
        break;
    }
    }
}
