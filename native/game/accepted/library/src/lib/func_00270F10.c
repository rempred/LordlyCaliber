#include "game/effect_geometry_e05.h"
#include "game/effect_callbacks_p02.h"

extern void *func_00261560(s32);
extern u8 p02_data_80220980[];

void func_00270F10(u16 message, P02StateF7F0 *state)
{
    if (message == 0) {
        u32 i;
        void *value;
        P02EntryF7F0 *object;

        value = func_00261560(3);
        for (i = 0; i < state->value_50->value_04; ++i) {
            if (state->value_58[i]) {
                state->value_58[i]->value_00 = (s32 (*)(P02EntryF7F0 *))func_00264b1c;
                object = state->value_58[i];
                object->value_34 = value;
                object->value_30 = p02_data_80220980;
                object->value_38 = object->value_3A = 0;
                object->value_3C = object->value_3E = 15;
                object->value_40 = object->value_42 = 0;
                object->value_44 = object->value_46 = 15;
            }
        }
    } else {
        /* The complete physical state prefix is shared with F7F0. */
        func_0026F7F0(message, state);
    }
}
