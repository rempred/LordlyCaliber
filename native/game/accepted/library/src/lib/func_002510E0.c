#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

typedef struct P05LoopState {
    u8 *context;
    u8 *values;
    u8 first;
    u8 unknown_09;
    s16 index;
} P05LoopState;


typedef struct P05ValueObject {
    u8 unknown_00[0xC];
    s32 value_0C;
} P05ValueObject;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
/* Descriptor 11 retains these views inside original data_0025e318. */
extern P05SixByteRecord D_80209970[];
extern u8 D_802099D8[];
extern u8 D_802099E4[];
extern u8 D_802099EC[];
extern void func_00262B68(void);





s32 func_002510E0(u16 message, P05LoopState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Scalar view preserves the observed first store/global-read order. */
        *(u8 **)state = p01_context_80220F68;
        state->values = p01_pointer_80220F6C;
        state->first = state->values[0];
        state->index = 0;
        if ((s16)func_0024BCA0(state->first) != 0) {
            state->index++;
        }
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001: {
        s16 index;

        func_0025D8A4((s16 *)(D_802099D8), (s16 *)(D_802099E4));
        for (index = 0; state->context[index] != 0xFF; index++) {
            func_0025CA7C(state->context[index], D_802099EC);
        }
        func_00262830(0x1002);
        /* The original immediately enters the next command's body. */
    }
        /* fall through */
    case 0x1002:
        func_0024E408(0x71);
        func_0024E9BC(0, state->context[0], 0x53);
        func_00262C64(0xE, 0x1003);
        break;
    case 0x1003: {
        P05SixByteRecord *records = D_80209970;
        P05ValueObject *object;

        object = func_0024DA10(0x21, &records[0]);
        object->value_0C = 30000;
        object = func_0024DA10(0x21, &records[1]);
        object->value_0C = 20000;
        object = func_0024DA10(0x21, &records[2]);
        object->value_0C = 10000;
        func_00262C64(0x4C, 0x1004);
        break;
    }
    case 0x1004:
        func_0024DA10(0x21, &D_80209970[3]);
        func_0024E9BC(0, state->context[0], 0x54);
        func_00262C64(4, 0x1005);
        break;
    case 0x1005:
        func_0024DA10(0x21, &D_80209970[4]);
        func_00262C64(0x1D, 0x1006);
        break;
    case 0x1006:
        func_0024E408(0x77);
        func_00262C64(0x23, 0x1007);
        break;
    case 0x1007:
        func_0024E408(0x78);
        func_00262830(0x1008);
        break;
    case 0x1008: {
        u8 value = state->values[state->index];

        if (value != 0xFF) {
            func_0024DA10(value, &D_80209970[5]);
            func_0025CAF0(value, 1, 0);
        }
        state->index++;
        if (state->values[state->index] == 0xFF) {
            func_00262C64(0x18, 0x1009);
        } else {
            func_00262C64(2, 0x1008);
        }
        break;
    }
    case 0x1009:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
