#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"

typedef struct P01ExtendedRootState {
    u8 *context;
    u8 *value_04;
    u8 value_08;
} P01ExtendedRootState;

struct P01PlacementState;
extern s32 func_00254220(u16, struct P01PlacementState *);
extern void func_00262B68(void);
extern void func_00262830(s32);

extern void func_00262C64(s16, s16);
extern void *func_00262AB0(void *);






extern void func_002628E4(s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_data_80209BD4[];
extern u8 p01_data_80209C24[];
extern u8 p01_data_80209C1C[];

s32 func_002548AC(u16 message, P01ExtendedRootState *state)
{
    u8 *value;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Keep the first context store before the following shared load. */
        *(u8 **)state = p01_context_80220F68;
        value = p01_pointer_80220F6C;
        state->value_04 = value;
        state->value_08 = *value;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        p01_context_80220F68 = state->context;
        *(u8 *)&p01_pointer_80220F6C = state->value_08;
        func_00262AB0((void *)func_00254220);
        func_0025DB00(state->context, (const struct Func0025DAB0Input *)(p01_data_80209C1C));
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, p01_data_80209BD4);
        func_0024DA10(state->value_08, p01_data_80209BD4 + 6);
        func_0024E9BC(0, state->context[0], 0x7D);
        func_0025CAF0(state->value_08, 0xF, 0xF);
        func_0025CB60(state->value_04 + 1, 0x14, 0xA);
        func_0025DAB0(state->value_08, p01_data_80209C24);
        func_00262C64(0x31, 0x1003);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
