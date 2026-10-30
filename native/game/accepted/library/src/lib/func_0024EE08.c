#include "common/types.h"

typedef struct P01RootState {
    u8 *context;
    u32 unknown_04;
    u8 value_08;
} P01RootState;

extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_0025C8A4(void *);
extern void func_00262C64(s16, s16);
extern void func_00262AB0(void *);
extern s32 func_0024EA70(u16, void *);
extern void *func_0024DA10(u32, void *);
extern void func_0024E9BC(s32, u8, s32);
extern void func_0025CAF0(u8, s32, s32);
extern void func_0025DAB0(u8, void *);
extern void func_002628E4(s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_data_80209786[];
extern u8 p01_data_802097D0[];

/* The dispatch saves two views of the shared presentation context.  The
 * initialization reads 0F6C as a pointer; the child handoff writes its first
 * byte, as in the retail accesses.  No stronger global layout is assumed. */
s32 func_0024EE08(u16 message, P01RootState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* The scalar view keeps this store before the following global load. */
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
        *(u8 *)&p01_pointer_80220F6C = state->value_08;
        func_00262AB0((void *)func_0024EA70);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, p01_data_80209786);
        func_0024E9BC(0, state->context[0], 0x28);
        func_0025CAF0(state->value_08, 8, 5);
        func_0025DAB0(state->value_08, p01_data_802097D0);
        func_00262C64(0x1C, 0x1003);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
