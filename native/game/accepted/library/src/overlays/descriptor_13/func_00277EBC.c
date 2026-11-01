#include "game/effect_presentation_p06_internal.h"

typedef struct P06State77EBC { u8 *value_00, *value_04; } P06State77EBC;
extern u8 *p06_pointer_80220F6C;
extern u8 p06_data_80225766[];
s32 func_00277EBC(u16 message, P06State77EBC *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_04 = p06_pointer_80220F6C;
        func_00262C64(10, 0x1000);
        break;
    case 0x1000:
        func_0024DC20(state->value_04[0], (struct Dc20InputRecord *)p06_data_80225766, 14);
        func_0024DA10(state->value_04[0], p06_data_80225766 + 6);
        func_00262830(0x101);
        break;
    }
    return 0;
}
