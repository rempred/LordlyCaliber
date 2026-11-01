#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"

typedef struct P01RootState {
    u8 *context;
    u32 unknown_04;
    u8 value_08;
} P01RootState;

extern void func_00262B68(void);
extern void func_00262830(s32);

extern void func_00262C64(s16, s16);
extern void *func_00262AB0(void *);
struct P01TrailState;
extern s32 func_002532A8(u16, struct P01TrailState *);




extern void func_002628E4(s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_data_80209B70[];

/* The dispatch saves two views of the shared presentation context.  The
 * initialization reads 0F6C as a pointer; the child handoff writes its first
 * byte, as in the retail accesses.  No stronger global layout is assumed. */
s32 func_00253610(u16 message, P01RootState *state)
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
        func_00262AB0((void *)func_002532A8);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0025DAB0(state->value_08, p01_data_80209B70);
        func_00262C64(0x13, 0x1003);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
