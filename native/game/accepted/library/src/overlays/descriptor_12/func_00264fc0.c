#include "game/effect_geometry_e05_internal.h"

void func_00264fc0(E05Header *input, E05Descriptor *output)
{
    if (input->value_00 != 0x36) goto invalid;
    if (input->value_01 != 0x34) goto invalid;
        output->data_00 = input + 1;
        output->value_04 = input->value_04;
        output->value_06 = input->value_06;
        output->value_08 = input->value_02;
        output->value_0A = input->value_03;
    goto done;
invalid:
    for (;;) {}
done:
    return;
}
