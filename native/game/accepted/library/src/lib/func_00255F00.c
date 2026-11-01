#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

typedef struct P05PointerState {
    u8 *context;
    u8 *values;
    u8 first;
} P05PointerState;


extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
/* Descriptor 11 views remain inside original data_0025e890. */
extern P05BuilderSpec D_80209D66[];
extern u8 D_80209DCC[];
extern u8 D_80209DD4[];
extern u8 D_80209DD8[];
extern u8 D_80209DE0[];
extern void func_00262B68(void);



s32 func_00255F00(u16 message, P05PointerState *state)
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
    case 0x1001:
        func_0025D8A4((s16 *)(D_80209DCC), (s16 *)(D_80209DD4));
        func_00262830(0x1002);
        /* fall through */
    case 0x1002:
        func_0024E490(state->first, (const u8 *)&D_80209D66[3], 2);
        func_0024E9BC(0, state->context[0], 0x8D);
        func_0025DAB0(state->first, (const struct Func0025DAB0Input *)D_80209DD8);
        func_00262C64(0x13, 0x1003);
        break;
    case 0x1003:
        func_0024DA10(state->first, &D_80209D66[1]);
        func_00262C64(9, 0x1004);
        break;
    case 0x1004:
        func_0024DA10(state->first, &D_80209D66[2]);
        func_00262C64(8, 0x1005);
        break;
    case 0x1005: {
        P05BuilderSpec *records = D_80209D66;

        func_0024DA10(state->first, &records[0]);
        func_0024DA10(state->first, &records[5]);
        func_0025CAF0(state->first, 0xA, 0xA);
        func_0025CB60((u32)(state->values + 1), 0xA, 0xA);
        func_0025DAB0(state->first, (const struct Func0025DAB0Input *)D_80209DE0);
        func_00262C64(0x22, 0x1006);
        break;
    }
    case 0x1006:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
