#include "game/effect_model_e06.h"
#include "game/effect_dispatch_e07.h"

typedef void (*Func00269470Callback)(u32 reason, void *fields, void *argument);

typedef struct Func0026A204Fields {
    u8 unknown_00[0x40];
    float value_40;
    u16 flags_44;
    u16 unknown_46;
    u32 value_48;
    u32 *record_4C;
    void *value_50;
    void *value_54;
    u16 value_58;
    u8 unknown_5A[0x12];
    Func00269470Callback callback_6C;
} Func0026A204Fields;

typedef struct Func0026A204State {
    u8 unknown_00[8];
    u16 flags_08;
    u16 flags_0A;
    void *value_0C;
    void *value_10;
    u8 unknown_14[8];
    Func0026A204Fields fields_1C;
} Func0026A204State;

struct Func0026A078Input;
struct Func0026A078Record;

extern u16 g_func_00269470_flags;
extern void *g_func_00269470_value_3C;
/* Paired with the case-0 halfword read to retain the observed load order. */
extern volatile Func00269470Callback g_func_00269470_callback;
extern void func_00028C40(void *fields);
u32 func_0026a204(u16 command, Func0026A204State *state)
{
    u32 result = 0;
    Func0026A204Fields *fields = &state->fields_1C;

    switch (command) {
    case 1:
        if ((fields->flags_44 & 1) && fields->callback_6C) {
            fields->callback_6C(1, fields, 0);
        }
        func_00268798((struct E06State *)(&fields->value_48));
        break;
    case 2:
        /* Each mask-0x4 path below is a local self-loop in the reference body. */
        if (fields->flags_44 & 4) {
            for (;;) {
            }
        }
        func_00268800((struct E06State *)(&fields->value_48));
        break;
    case 6:
        result = fields->record_4C[0];
        break;
    case 0: {
        u16 initial_flags;

        fields->flags_44 = g_func_00269470_flags;
        /* Preserve the observed halfword reload after its store. */
        initial_flags = *(volatile u16 *)&fields->flags_44;
        fields->callback_6C = g_func_00269470_callback;
        if (initial_flags & 4) {
            for (;;) {
            }
        }
        func_00268678((struct E06State *)(&fields->value_48), (s32)(g_func_00269470_value_3C));
        func_00028C40(fields);
        fields->value_40 = 1.0f;
        if ((fields->flags_44 & 1) && fields->callback_6C) {
            fields->callback_6C(0, fields, 0);
        }
        break;
    }
    case 5:
        func_00268358((s32)(fields->record_4C[1]), (void **)(fields->value_54));
        state->flags_08 &= ~4;
        break;
    case 3:
        func_00267FD0((s32)(state->value_10), (struct E06TransformInput *)(fields->record_4C), (struct E06TransformOutput *)(fields->value_50));
        func_002682A4((float (*)[4])(fields), (struct E06HierarchyInput *)(fields->record_4C), (struct E06TransformOutput *)(fields->value_50));
        func_00268400((s32)(fields->record_4C[1]), (struct E06TransformOutput *)(fields->value_50), (struct E06TripleNode **)(fields->value_54));
        if (state->flags_0A & 0x1000) {
            func_002677D0((s32)(fields->record_4C[1]), (struct E06TripleNode **)(fields->value_54), (s32)(state->value_0C));
        }
        func_0026a078((s32)state->value_10,
                     (struct Func0026A078Input *)fields->record_4C,
                     (struct Func0026A078Record *)fields->value_50);
        break;
    case 4:
        if (fields->flags_44 & 2) {
            func_00268478((s32)(fields->record_4C[1]), (struct E06TransformOutput *)(fields->value_50), fields->value_40);
        }
        if (fields->value_58) {
            if (fields->flags_44 & 4) {
                for (;;) {
                }
            }
            func_00268F64((s32)(state->value_10), (struct E06Model *)(fields->record_4C), (struct E06Node **)(fields->value_54));
        }
        if ((fields->flags_44 & 1) && fields->callback_6C) {
            fields->callback_6C(2, fields, state->value_10);
        }
        break;
    }
    return result;
}
