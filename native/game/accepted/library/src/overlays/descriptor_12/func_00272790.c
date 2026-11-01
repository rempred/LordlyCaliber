#include "game/effect_commands_e09.h"
#include "game/effect_presentation_p04.h"

typedef struct E09State72790 {
    /* Retain the initial store before loading the second word parameter. */
    volatile u32 value_00;
    u32 value_04;
    void *value_08[6];
} E09State72790;
typedef struct E09Task72790 {
    u8 unknown_00[0x14];
    void *value_14;
} E09Task72790;
extern E09Task72790 *e09_task_80220FE8;
extern volatile u32 g_func_0025CB60_pointer;
extern s16 *volatile e09_pointer_80220F68;
extern void bzero(void *, u32);
extern s32 func_0026285C(void *, u16);
extern void func_00262B68(void);
extern s32 func_002623E0(void *);
extern void func_002623C4(void *);

s32 func_00272790(u16 command, E09State72790 *state)
{
    u32 i;
    s32 count;
    switch (command) {
    case 0x101:
        func_0026285C(e09_task_80220FE8->value_14, 0x1004);
        func_00262B68();
        break;
    case 0x100:
        bzero(state, 32);
        state->value_00 = g_func_0025CB60_pointer;
        state->value_04 = (u32)e09_pointer_80220F68;
        func_00262830(0x1000);
        break;
    case 0x100A:
        for (i = 0; i < 6; i++) {
            if (state->value_08[i] && func_002623E0(state->value_08[i])) {
                func_002623C4(state->value_08[i]);
                state->value_08[i] = 0;
            }
        }
        i = 0;
        count = 0;
        for (; i < 6; i++) count += state->value_08[i] != 0;
        if (count == 0) func_00262830(0x101);
        break;
    }
    return 0;
}
