#include "game/effect_math_e04.h"

typedef struct Func001F0E64Record Func001F0E64Record;

extern void *func_0020C478(unsigned int);
extern void func_001F0E64(Func001F0E64Record *, unsigned int);

void func_0026C90C(unsigned int index, unsigned int value)
{
    void **objects = func_0020C478(index);
    unsigned int i;

    for (i = 0; i < 3; i++) {
        if (objects[i] != 0) {
            func_001F0E64(
                (Func001F0E64Record *)((unsigned char *)objects[i] + 0x44),
                value);
        }
    }
}
