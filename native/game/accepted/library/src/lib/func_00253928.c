#include "common/types.h"

typedef struct P01BytePairState {
    u8 value_00;
    u8 value_01;
} P01BytePairState;

extern u8 p01_byte_80220F68;
extern u8 p01_byte_80220F69;
extern u8 p01_data_80209B38[];
extern u8 p01_data_80209B3E[];
extern u8 p01_data_80209B44[];
extern u8 p01_data_80209B80[];
extern u8 p01_data_80209B88[];
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_00262C64(s16, s16);
extern void func_002628E4(s32);
extern void func_00262C80(void);
extern u32 func_0024BCA0(u32);
extern s16 func_0024C3EC(u8);
extern void func_0024DC20(u8, void *, s32);
extern void func_0024E9BC(s32, u8, s32);
extern void func_0025DAB0(u8, void *);
extern void *func_0024DA10(u32, void *);
extern void func_0025CA7C(u8, void *);

s32 func_00253928(u16 message, P01BytePairState *state)
{
    u8 value;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_00 = p01_byte_80220F68;
        state->value_01 = p01_byte_80220F69;
        func_00262830(0x1000);
        break;
    case 0x1000:
        if ((s16)func_0024BCA0(state->value_01) == 0) {
            value = func_0024C3EC(state->value_01);
        } else {
            value = state->value_01;
        }
        func_0024DC20(value, p01_data_80209B38, 3);
        func_0024E9BC(0, state->value_00, 0x73);
        func_0025DAB0(state->value_01, p01_data_80209B80);
        func_00262C64(0xE, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_01, p01_data_80209B3E);
        func_0025CA7C(state->value_01, p01_data_80209B88);
        func_00262C64(0x20, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_01, p01_data_80209B44);
        func_00262C64(0x26, 0x1003);
        break;
    case 0x1003:
        func_00262C80();
        func_002628E4(0x101);
        break;
    }
    return 0;
}
