#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"

typedef struct E09TaskC6A4 E09TaskC6A4;
struct E09TaskC6A4 {
    u32 value_00;
    s32 (*value_04)(s32, E09TaskC6A4 *);
};

typedef struct E09StateC6A4 {
    u8 *value_00;
} E09StateC6A4;

/* These are address views, not independent allocation extents. */
extern volatile void *g_func_0025C8A4_saved_arg;
extern u16 e09_u16_8020A1C0;
extern s16 e09_s16_8020A2D2;
extern const u8 D_800EB290[];
extern void func_00262B68(void);
extern void func_00262C80(void);


s32 func_0025C6A4(u16 command, E09StateC6A4 *state)
{
    s32 result = 0;
    u16 *field;
    E09TaskC6A4 *task;
    s16 index;
    s16 value;
    s32 send_arg;
    s32 constant_8;

    switch (command) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_00 = (void *)g_func_0025C8A4_saved_arg;
        func_00262830(0x1000);
        break;
    case 0x1000:
        task = 0;
        for (index = 0; state->value_00[index] != 0xFF; index++) {
            value = func_0025F74C(state->value_00[index]);
            /* Keep the tag separate: KMC hoists it before the field address. */
            constant_8 = 8;
            field = &e09_u16_8020A1C0;
            if (value == constant_8) {
                send_arg = 20;
                goto send;
            }
            if (value > 0) *field = value + 3;
            else *field = 4;
            task = func_0024DA10(state->value_00[index], field);
            if (*field == 10) break;
        }
        func_0024E9BC((s32)D_800EB290, state->value_00[0],
                     func_0025C590(e09_s16_8020A2D2));
        send_arg = (s16)task->value_04(6, task);
send:
        func_00262C64(send_arg, 0x1001);
        return result;
    case 0x1001:
        func_00262C80();
        func_00262830(0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return result;
}
