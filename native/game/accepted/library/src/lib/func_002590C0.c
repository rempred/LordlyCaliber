#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

/* The producer writes at most nine entries and its 0xFF terminator. */
typedef struct P05ScanState {
    u8 values[10];
    s16 index;
} P05ScanState;


extern u8 p01_byte_80220F68;
extern u32 func_00022e60(void);
extern void func_00262B68(void);
extern P05FactoryRecord D_80209F20[];

s32 func_002590C0(u16 message, P05ScanState *state)
{
    s16 kind;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        func_0024C428(state->values, p01_byte_80220F68);
        state->index = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        kind = (s32)func_00022e60() % 3;
        if (state->values[state->index] != 0xFF) {
            func_0024DA10(state->values[state->index], &D_80209F20[kind]);
            if (kind == 2) {
                p01_byte_80220F68 = state->values[state->index];
                func_00262AB0((void *)func_00259FD4);
            }
            state->index++;
            func_00262C64(4, 0x1000);
        } else {
            func_00262C64(0x2D, 0x1001);
        }
        break;
    case 0x1001:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
