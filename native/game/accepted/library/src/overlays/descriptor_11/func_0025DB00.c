#include "game/effect_commands_e09.h"
#include "game/effect_callbacks_p02.h"

extern volatile u32 g_func_0025CB60_pointer;
extern volatile u16 g_func_0025DAB0_value_0F70;
extern volatile u16 g_func_0025DAB0_value_0F72;
extern volatile u8 g_func_0025DAB0_value_0F74;


void func_0025DB00(void *value, const Func0025DAB0Input *input)
{
    /* This access view retains both halfword load/store pairs in KMC. */
    volatile u16 *halfwords = (volatile u16 *)input;
    g_func_0025CB60_pointer = (u32)value;
    g_func_0025DAB0_value_0F70 = halfwords[0];
    g_func_0025DAB0_value_0F72 = halfwords[1];
    g_func_0025DAB0_value_0F74 = ((u8 *)halfwords)[4] | 0x20;
    func_00262AB0((void *)func_0025D8D4);
}
