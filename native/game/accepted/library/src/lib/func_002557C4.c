#include "common/types.h"

typedef struct P03LoopState {
    u8 *context;
    u8 *values;
    u8 first;
    u8 unknown_09;
    s16 index;
} P03LoopState;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p03_data_80209CBC[];
extern u8 p03_data_80209D44[];
extern u8 p03_data_80209D4C[];
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_002628E4(s32);
extern void func_00262C64(s16, s16);
extern u32 func_0024BCA0(u32);
extern void func_0025C8A4(void *);
extern void func_0025DAB0(u8, void *);
extern void *func_0024DA10(u32, void *);
extern void func_0024E9BC(s32, u8, s32);
extern void func_0025CAF0(u8, s32, s32);
extern void func_0024E898(u8, s32);
extern void func_0024DC20(u8, void *, s32);
extern void func_00262AB0(void *);
extern s32 func_0025630C(u16, u8 *);

s32 func_002557C4(u16 message, P03LoopState *state)
{
    u8 value;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* The first scalar pointer store preserves the observed access order. */
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
    case 0x1001:
        func_0025DAB0(state->first, p03_data_80209D44);
        func_00262830(0x1002);
        /* fall through */
    case 0x1002:
        value = state->values[state->index];
        if (value != 0xFF) {
            func_0024DA10(value, p03_data_80209CBC);
            func_0024DC20(value, p03_data_80209CBC + 0x12, 7);
            *(u8 *)&p01_context_80220F68 = value;
            func_00262AB0((void *)func_0025630C);
            func_0024E9BC(0, state->context[0], 0x87);
            func_0025CAF0(value, 0x1E, 7);
            func_0024E898(value, 0xFF);
        }
        state->index++;
        if (state->values[state->index] == 0xFF) {
            func_0025DAB0(value, p03_data_80209D4C);
            func_00262C64(0x39, 0x1003);
        } else {
            func_00262C64(0x14, 0x1002);
        }
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
