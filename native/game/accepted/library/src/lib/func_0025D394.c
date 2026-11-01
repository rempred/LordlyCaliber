#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

struct P05StateCDB4;
extern u8 *p01_pointer_80220F6C;

void func_0025D394(u8 *values)
{
    if (values[0] != 0xFF) {
        p01_pointer_80220F6C = values;
        func_00262AB0((void *)func_0025CDB4);
    }
}
