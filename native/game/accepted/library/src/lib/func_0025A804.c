#include "game/effect_model_e06.h"
#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

typedef struct P05FlagObject {
    u8 unknown_00[0xA];
    u16 flags_0A;
} P05FlagObject;
/* Numeric fields only; neither view defines the allocation extent. */
typedef struct P05ObjectRootState {
    u8 *context;
    u8 *values;
    u8 unknown_08[4];
    u8 value_0C;
    u8 unknown_0D[3];
    P05FlagObject *object;
} P05ObjectRootState;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern P05FactoryRecord D_8020A07C[];
extern u8 D_8020A13C[];
extern u8 D_8020A144[];
extern void func_00262B68(void);



s32 func_0025A804(u16 message, P05ObjectRootState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)state = p01_context_80220F68;
        state->values = p01_pointer_80220F6C;
        state->value_0C = state->values[0];
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025D8A4((s16 *)(D_8020A13C), (s16 *)(D_8020A144));
        func_00262830(0x1001);
        /* Retail continues into the next case in this invocation. */
    case 0x1001: {
        P05FactoryRecord *record = &D_8020A07C[0];

        state->object = func_0024DA10(state->value_0C, record);
        func_0024DA50(state->value_0C, record + 1);
        func_0024DA10(state->value_0C, record + 3);
        func_0024E9BC(0, state->context[0], 0xAF);
        func_00262C64(0x11, 0x1002);
        break;
    }
    case 0x1002:
        func_0024DA10(state->value_0C, &D_8020A07C[2]);
        func_00262C64(0x12, 0x1003);
        break;
    case 0x1003:
        func_00269324((struct E06RepackStateA *)(state->object));
        state->object->flags_0A &= 0xEFFF;
        func_0025CAF0(state->value_0C, 0xF, 5);
        func_0025CB60((u32)(state->values + 1), 0x12, 0);
        func_00262C64(0x23, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
