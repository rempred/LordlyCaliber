#include "common/types.h"

typedef struct P01RootState {
    u8 *context;
    u32 unknown_04;
    u8 value_08;
} P01RootState;

typedef struct P01SixByteRecord {
    u8 bytes[6];
} P01SixByteRecord;

struct P01F730State;
extern s32 func_0024F730(u16, struct P01F730State *);
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_0025C8A4(void *);
extern void func_00262C64(s16, s16);
extern void func_00262AB0(void *);
extern void *func_0024DA10(u32, void *);
extern void func_0024E9BC(s32, u8, s32);
extern void func_0025CAF0(u8, s32, s32);
extern void func_0025DAB0(u8, void *);
extern void func_002628E4(s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F6D;
extern P01SixByteRecord *p01_pointer_80220F70;
extern P01SixByteRecord p01_records_80209810[];
extern u8 p01_data_80209816[];
extern u8 p01_data_8020984C[];

s32 func_0024FA70(u16 message, P01RootState *state)
{
    u8 value;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Preserve the first store before the shared pointer read. */
        *(u8 **)state = p01_context_80220F68;
        state->value_08 = *p01_pointer_80220F6C;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        p01_context_80220F68 = state->context;
        value = state->value_08;
        p01_byte_80220F6D = 1;
        p01_pointer_80220F70 = p01_records_80209810;
        *(u8 *)&p01_pointer_80220F6C = value;
        func_00262AB0((void *)func_0024F730);
        func_0024E9BC(0, state->context[0], 0x31);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, p01_data_80209816);
        func_0024E9BC(0, state->context[0], 0x32);
        func_0025CAF0(state->value_08, 5, 0xA);
        func_0025DAB0(state->value_08, p01_data_8020984C);
        func_00262C64(0x1C, 0x1003);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
