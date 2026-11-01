#include "game/effect_commands_e09.h"
#include "game/effect_callbacks_p02.h"
#include "game/effect_adapters_e08.h"


extern volatile u8 e09_byte_80220F68;
extern volatile u8 e09_byte_80220F69;
extern volatile u8 e09_byte_80220F6A;


void func_0025CA7C(u8 value, u8 *input)
{
    if ((s16)func_0024BCA0(value & 255) == 0) {
        e09_byte_80220F68 = value;
        e09_byte_80220F69 = input[0];
        e09_byte_80220F6A = input[1] - input[0];
        func_00262AB0((void *)func_0025C8D0);
    }
}
