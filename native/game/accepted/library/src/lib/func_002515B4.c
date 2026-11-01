#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P05LoopState {
    u8 *context;
    u8 *values;
    u8 first;
    u8 unknown_09;
    s16 index;
} P05LoopState;


extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F69;
/* Descriptor 11 views remain inside original data_0025e318. */
extern P05SixByteRecord D_802099AC[];
extern u8 D_802099FC[];
extern u8 D_80209A04[];
extern u8 D_80209A08[];
extern u8 D_80209A0C[];
extern s16 D_80209A10[];
extern void func_00262B68(void);






s32 func_002515B4(u16 message, P05LoopState *state)
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
        state->index = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001: {
        s16 index;

        func_0025D8A4((s16 *)(D_802099FC), (s16 *)(D_80209A04));
        func_0025D678(D_80209A08);
        for (index = 0; state->context[index] != 0xFF; index++) {
            func_0025CA7C(state->context[index], D_80209A0C);
        }
        func_00262830(0x1002);
    }
        /* fall through */
    case 0x1002:
        func_0024E408(0x7F);
        func_0024E9BC(0, state->context[0], 0x55);
        func_00262C64(0x28, 0x1003);
        break;
    case 0x1003:
        func_0024DA10(0x21, &D_802099AC[2]);
        func_00262C64(4, 0x1004);
        break;
    case 0x1004:
        func_0024DA10(0x21, &D_802099AC[0]);
        func_00262C64(9, 0x1005);
        break;
    case 0x1005:
        func_0024DC20(0x21, (struct Dc20InputRecord *)(&D_802099AC[1]), 1);
        func_00262C64(0x32, 0x1006);
        break;
    case 0x1006:
        func_00262AB0((void *)func_002520C4);
        func_0024E9BC(0, state->context[0], 0x56);
        func_00262C64(0xF, 0x1007);
        break;
    case 0x1007:
        func_00262AB0((void *)func_0025145C);
        func_00262830(0x1008);
        /* fall through */
    case 0x1008:
        p01_byte_80220F69 = state->first;
        /* Scalar view retains the observed byte-store/halfword-load order. */
        p05_halfword_80220F6A = *(s16 *)((u8 *)state + 0xA);
        func_00262AB0((void *)func_00252198);
        if (state->index == 2) {
            func_0025CAF0(state->first, 0x14, 0xF);
            func_0025CB60((u32)(state->values + 1), 0x14, 0xF);
            func_00262C64(-1, 0x1009);
        } else {
            func_00262C64(D_80209A10[state->index], 0x1008);
        }
        state->index++;
        break;
    case 0x1009:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
