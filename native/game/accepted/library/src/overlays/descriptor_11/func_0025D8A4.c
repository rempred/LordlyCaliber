#include "game/effect_commands_e09.h"
#include "game/effect_callbacks_p02.h"
extern s16 *e09_pointer_80220F6C;
extern s16 *volatile e09_pointer_80220F68;


void func_0025D8A4(s16 *first, s16 *second)
{
    e09_pointer_80220F6C = first;
    e09_pointer_80220F68 = second;
    func_00262AB0((void *)func_0025D6C8);
}
