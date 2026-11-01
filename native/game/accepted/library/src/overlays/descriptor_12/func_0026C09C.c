#include "game/effect_model_e06.h"
#include "game/effect_dispatch_e07.h"

typedef struct Func0026C09CFields {
    float matrix_00[4][4];
    u32 work_40;
    u32 *record_44;
    void *records_48;
    void *slots_4C;
    u16 value_50;
} Func0026C09CFields;

/* This describes the accessed fields, not the complete allocation. */
typedef struct Func0026C09CState {
    u8 unknown_00[8];
    u16 flags_08;
    u16 flags_0A;
    void *value_0C;
    void *value_10;
    u8 unknown_14[8];
    Func0026C09CFields fields_1C;
} Func0026C09CState;

extern volatile u32 g_func_0026B820_value_38;
extern void func_00028C40(void *matrix);
u32 func_0026C09C(u16 command, Func0026C09CState *state)
{
    u32 result = 0;
    Func0026C09CFields *fields = &state->fields_1C;

    switch (command) {
    case 1:
        func_00268E6C((struct E06State *)(&fields->work_40));
        break;
    case 2:
        func_00268ED4((struct E06State *)(&fields->work_40));
        break;
    case 6:
        result = fields->record_44[0];
        break;
    case 0:
        func_00268D4C((struct E06State *)(&fields->work_40), (s32)(g_func_0026B820_value_38));
        func_00028C40(fields->matrix_00);
        break;
    case 5:
        func_00267750((s32)(fields->record_44[1]), (struct E06FlagNode **)(fields->slots_4C));
        state->flags_08 &= ~4;
        break;
    case 3:
        func_00267FD0((s32)(state->value_10), (struct E06TransformInput *)(fields->record_44), (struct E06TransformOutput *)(fields->records_48));
        func_002682A4((float (*)[4])(fields->matrix_00), (struct E06HierarchyInput *)(fields->record_44), (struct E06TransformOutput *)(fields->records_48));
        func_002683B4((s32)(fields->record_44[1]), (struct E06TransformOutput *)(fields->records_48), (struct E06FloatNode **)(fields->slots_4C));
        if (state->flags_0A & 0x1000) {
            func_00267788((s32)(fields->record_44[1]), (struct E06FloatNode **)(fields->slots_4C), (s32)(state->value_0C));
        }
        break;
    case 4:
        if (fields->value_50) {
            func_002691EC((s32)(state->value_10), (struct E06Model *)(fields->record_44), (struct E06RenderNode **)(fields->slots_4C));
        }
        break;
    }
    return result;
}
