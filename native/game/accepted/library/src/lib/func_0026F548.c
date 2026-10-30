#include "game/effect_callbacks_p02.h"

typedef struct P02StateF1E0 P02StateF1E0;
extern s32 p02_word_80220F68[], p02_word_80220F6C[];

void *func_0026F548(s32 first, s32 second)
{
    p02_word_80220F68[0] = first;
    p02_word_80220F6C[0] = second;
    return func_00262AB0((void *)func_0026F1E0);
}
