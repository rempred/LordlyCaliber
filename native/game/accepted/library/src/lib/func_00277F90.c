#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;

/* Only the numeric prefix used by this dispatcher is represented. */


extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
/* Original data_00279da8 retains the six-byte records and two byte views. */
extern P05FactoryRecord D_80225780[];
extern u8 D_802257C8[], D_802257D0[];
extern void func_00262B68(void);






s32 func_00277F90(u16 message, P05State77F90 *state)
{
    P01Vec3 position;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Address views keep the subsequent list read separate from its store. */
        *(u8 **)state = p01_context_80220F68;
        *(u8 **)((u8 *)state + 4) = p01_pointer_80220F6C;
        state->index = 0;
        if ((s16)func_0024BCA0(state->values[0]) != 0) {
            state->index++;
        }
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0025D8A4((s16 *)(D_802257C8), (s16 *)(D_802257D0));
        func_00262830(0x1003);
        break;
    case 0x1003:
        func_0024E9BC(0, state->context[0], 0x5F);
        func_0024BF04(state->values[0], &position, 0);
        func_0024D41C(&position, &D_80225780[1]);
        func_0024D180(&position, &D_80225780[3]);
        func_00262C64(9, 0x1004);
        break;
    case 0x1004:
        func_0024BF04(state->values[0], &position, 0);
        func_0026CCB4(position.x, position.y, position.z, 200);
        func_0024D41C(&position, &D_80225780[5]);
        func_00262C64(1, 0x1005);
        break;
    case 0x1005:
        func_0024BF04(state->values[0], &position, 0);
        func_0024D180(&position, &D_80225780[4]);
        func_00262C64(3, 0x1006);
        break;
    case 0x1006:
        func_0024BF04(state->values[0], &position, 0);
        func_0024D41C(&position, &D_80225780[0]);
        func_00262C64(0x13, 0x1007);
        break;
    case 0x1007:
        func_0024BF04(state->values[0], &position, 0);
        func_0024D180(&position, &D_80225780[2]);
        func_00262C64(1, 0x1008);
        break;
    case 0x1008:
        func_0025CB60((u32)(state->values + state->index), 0x14, 0xF);
        func_00262C64(0x2D, 0x1009);
        break;
    case 0x1009:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
