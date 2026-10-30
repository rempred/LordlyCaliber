#include "game/effect_math_e04.h"

extern void *func_0020C478(unsigned int);
extern void func_00023460(const void *, void *, unsigned int);

void func_0026C994(unsigned int index, void *destination)
{
    void **objects = func_0020C478(index);

    func_00023460((unsigned char *)objects[0] + 0x44, destination, 0x50);
}
