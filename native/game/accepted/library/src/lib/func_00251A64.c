#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P05PointerState {
    u8 *context;
    u8 *values;
    u8 first;
} P05PointerState;


extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
/* Descriptor 11 views remain inside original data_0025e318. */
extern P05SixByteRecord D_802099BE[];
extern u8 D_80209A14[];
extern u8 D_80209A1C[];
extern u8 D_80209A20[];
extern u8 D_80209A24[];
extern void func_00262B68(void);





s32 func_00251A64(u16 message, P05PointerState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Preserve the observed first store before the shared-pointer load. */
        *(u8 **)state = p01_context_80220F68;
        state->values = p01_pointer_80220F6C;
        state->first = state->values[0];
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001: {
        s16 index;

        func_0025D8A4((s16 *)(D_80209A14), (s16 *)(D_80209A1C));
        for (index = 0; state->context[index] != 0xFF; index++) {
            func_0025CA7C(state->context[index], D_80209A20);
        }
        func_00262830(0x1002);
    }
        /* fall through */
    case 0x1002:
        func_0024DA10(0x18, &D_802099BE[0]);
        func_0024E9BC(0, state->context[0], 0x57);
        func_00262C64(0x14, 0x1003);
        break;
    case 0x1003:
        func_0024DA10(0x2A, &D_802099BE[0]);
        func_00262C64(5, 0x1004);
        break;
    case 0x1004:
        func_0025D678(D_80209A24);
        func_0024DA10(0x18, &D_802099BE[1]);
        func_00262C64(0x14, 0x1005);
        break;
    case 0x1005:
        func_0025D678(D_80209A24 + 4);
        func_0024DA10(0x2A, &D_802099BE[1]);
        func_00262C64(0xB, 0x1006);
        break;
    case 0x1006:
        func_00262AB0((void *)func_002518EC);
        func_0024DA10(0x1F, &D_802099BE[2]);
        func_0024E9BC(0, state->context[0], 0x58);
        func_00262C64(0x45, 0x1007);
        break;
    case 0x1007:
        func_0024DA10(0x1F, &D_802099BE[3]);
        func_0025CAF0(state->first, 0x1E, 0);
        func_0025CB60((u32)(state->values + 1), 0x1E, 0);
        func_00262C64(0x28, 0x1008);
        break;
    case 0x1008:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
