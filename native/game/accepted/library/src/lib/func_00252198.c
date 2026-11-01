#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

typedef struct P05IndexedByteState {
    u8 value_00;
    u8 value_01;
    s16 index_02;
} P05IndexedByteState;


extern u8 p01_byte_80220F69;
/* Descriptor 11 views remain inside original data_0025e318. */
extern u16 D_802099F4[];
extern P05SixByteRecord D_802099A0[];
extern void func_00262B68(void);
extern void func_00262C80(void);


s32 func_00252198(u16 message, P05IndexedByteState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Scalar view preserves the first byte store before the shared read. */
        ((u8 *)state)[1] = p01_byte_80220F69;
        state->index_02 = p05_halfword_80220F6A;
        state->value_00 = D_802099F4[state->index_02];
        if (func_0024BCE8(state->value_01) != 0) {
            state->value_00 += 0x12;
        }
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024DA10(state->value_00, &D_802099A0[1]);
        func_00262C64(0x11, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_00, &D_802099A0[0]);
        func_00262C64(0x22, 0x1002);
        break;
    case 0x1002:
        if (state->index_02 == 2) {
            func_00262C80();
        }
        func_002628E4(0x101);
        break;
    }
    return 0;
}
