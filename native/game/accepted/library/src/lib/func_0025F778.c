#include "game/effect_math_e04.h"

extern void *func_0020C478(unsigned int index);

int func_0025F778(unsigned int index)
{
    unsigned char *value = func_0020C478(index);

    return *(int *)(value + 0x5C);
}
