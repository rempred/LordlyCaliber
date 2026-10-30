#include "game/effect_callbacks_p02.h"

extern void *func_00261560(s32);
extern void func_00260C24(void *);
extern s32 func_00264b50(P02EntryF7F0 *);
extern u8 p02_data_80220980[];

void func_0026F7F0(u16 message, P02StateF7F0 *state)
{
    switch (message) {
    case 0: {
        u32 i;
        void *value;
        P02EntryF7F0 *object;
        value = func_00261560(3);
        for (i = 0; i < state->value_50->value_04; ++i) {
            if (state->value_58[i]) {
                state->value_58[i]->value_00 = func_00264b50;
                object = state->value_58[i];
                object->value_34 = value;
                object->value_30 = p02_data_80220980;
                object->value_38 = object->value_3A = 0;
                object->value_3C = object->value_3E = 15;
                object->value_40 = object->value_42 = 0;
                object->value_44 = object->value_46 = 15;
            }
        }
        break;
    }
    case 1: {
        s32 i = 0;
        while (!state->value_58[i])
            ++i;
        func_00260C24(state->value_58[i]->value_34);
        break;
    }
    case 2: {
        u32 i;
        P02EntryF7F0 *object;
        for (i = 0; i < state->value_50->value_04; ++i) {
            object = state->value_58[i];
            if (object) {
                object->value_42 = (object->value_42 + 1) & 15;
                object->value_44 = object->value_40 + 16;
                object->value_46 = object->value_42 + 15;
            }
        }
        break;
    }
    }
}
