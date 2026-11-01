#include "game/effect_presentation_p06_internal.h"

#include "game/effect_commands_e09.h"
#include "game/effect_presentation_p04.h"

typedef struct P06State78B00 { u8 *value_00, *value_04; } P06State78B00;
typedef struct P06Vec78B00 { float x, y, z; } P06Vec78B00;
extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C;
extern u8 p06_byte_80220F6C;
extern u8 p06_data_802257E0[], p06_data_802257EC[], p06_data_802257F2[];
s32 func_00278B00(u16 message, P06State78B00 *state)
{
    P06Vec78B00 position;
    s32 first;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)state = p06_pointer_80220F68;
        state->value_04 = p06_pointer_80220F6C;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->value_00);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0024E9BC(0, state->value_00[0], 0x68);
        func_0024C204(state->value_00, (struct P01Vec3 *)&position, 0x20);
        func_0024D180(&position, p06_data_802257E0);
        func_00262C64(12, 0x1004);
        break;
    case 0x1004:
        /* Preserve the context word and byte commits before both reloads. */
        p06_pointer_80220F68 = *(u8 **)state;
        p06_byte_80220F6C = (*(u8 **)((u8 *)state + 4))[0];
        if ((*(u8 **)state)[0] != (*(u8 **)((u8 *)state + 4))[0]) {
            func_00262AB0(func_00278560);
            func_00262C64(-1, 0x1005);
        } else {
            func_00262830(0x1006);
        }
        break;
    case 0x1005:
        func_0024E9BC(0, state->value_00[0], 0x69);
        first = func_00279920(state->value_00[0]);
        if (func_00279920(state->value_04[0]) < first)
            func_0024DA10(state->value_04[0], p06_data_802257EC);
        else
            func_0024DA10(state->value_04[0], p06_data_802257F2);
        func_0025CAF0(state->value_04[0], 5, 15);
        func_0025CB60((u32)(state->value_04 + 1), 15, 15);
        func_00262C64(65, 0x1006);
        break;
    case 0x1006:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
