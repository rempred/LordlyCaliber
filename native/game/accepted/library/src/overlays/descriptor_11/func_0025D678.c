#include "game/effect_commands_e09.h"
#include "game/effect_callbacks_p02.h"
extern u8 e09_byte_80220F68, e09_byte_80220F69;
extern u8 e09_byte_80220F6A, e09_byte_80220F6B;


void func_0025D678(u8 *input)
{
    e09_byte_80220F68 = input[0];
    e09_byte_80220F69 = input[1];
    e09_byte_80220F6A = input[2];
    e09_byte_80220F6B = input[3];
    func_00262AB0((void *)func_0025D3CC);
}
