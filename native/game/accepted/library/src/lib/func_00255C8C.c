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
extern u8 D_80209DB0[];
extern u8 D_80209DB8[];
extern u8 D_80209DBC[];
extern u8 D_80209DC4[];
extern void func_00262B68(void);



s32 func_00255C8C(u16 message, P05PointerState *state)
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
        func_0025D8A4((s16 *)(D_80209DB0), (s16 *)(D_80209DB8));
        func_00262830(0x1002);
        /* fall through */
    case 0x1002: {
        float scale = 0.75f;
        P05BuilderSpec *records = &D_80209D66[3];

        func_0024DA90(state->first, &records[0], scale);
        func_0024DA90(state->first, &records[1], scale);
        func_0024E9BC(0, state->context[0], 0x8C);
        func_0025DAB0(state->first, (const struct Func0025DAB0Input *)D_80209DBC);
        func_00262C64(0x13, 0x1003);
        break;
    }
    case 0x1003:
        func_0024DA90(state->first, &D_80209D66[1], 0.75f);
        func_00262C64(9, 0x1004);
        break;
    case 0x1004:
        func_0024DA90(state->first, &D_80209D66[2], 0.75f);
        func_00262C64(8, 0x1005);
        break;
    case 0x1005: {
        float scale = 0.75f;
        P05BuilderSpec *records = D_80209D66;

        func_0024DA90(state->first, &records[0], scale);
        func_0024DA90(state->first, &records[5], scale);
        func_0025CAF0(state->first, 0xA, 0xA);
        func_0025CB60((u32)(state->values + 1), 0xA, 0xA);
        func_0025DAB0(state->first, (const struct Func0025DAB0Input *)D_80209DC4);
        func_00262C64(0x22, 0x1006);
        break;
    }
    case 0x1006:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
