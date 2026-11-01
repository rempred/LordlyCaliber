#include "game/effect_dispatch_e07.h"

struct Func00269F00Curve;

typedef struct Func00269F00Input {
    u32 unknown_00;
    u32 count_04;
    struct Func00269F00Curve ***value_08;
    u8 unknown_0C[0x0C];
    float **value_18;
} Func00269F00Input;

typedef struct Func00269F00Output {
    float value_00[3];
    u8 unknown_0C[0x38];
} Func00269F00Output;

/* The accepted logical entry includes the mtc1 preamble at ROM 0x267990. */


void func_00269F00(s32 argument, Func00269F00Input *input,
                   Func00269F00Output *output)
{
    float values[3];
    float value = (float)argument / 30.0f;
    u32 item;
    u32 component;
    u32 source_component;

    for (item = 0; item < input->count_04; item++) {
        /* Separate source and destination induction retains the retail loops. */
        for (source_component = 0, component = 0; component < 3;
             component++, source_component++) {
            values[component] = input->value_18[item][source_component];
        }
        if (input->value_08[item] != 0) {
            for (component = 0, source_component = 0; component < 3;
                 component++, source_component++) {
                if (input->value_08[item][source_component] != 0) {
                    values[component] =
                        func_00267990(input->value_08[item][source_component], value);
                }
            }
        }
        for (component = 0; component < 3; component++) {
            output[item].value_00[component] = values[component];
        }
    }
}
