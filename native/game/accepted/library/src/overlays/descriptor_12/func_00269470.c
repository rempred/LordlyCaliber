#include "game/effect_model_e06_internal.h"

extern volatile u16 g_func_00269470_flags;
extern volatile Func00269470Callback g_func_00269470_callback;
extern volatile u16 g_func_00269470_value_44;
extern volatile u16 g_func_00269470_value_46;
extern void * volatile g_func_00269470_value_48;
extern void * volatile g_func_00269470_value_4C;
extern void * volatile g_func_00269470_value_3C;

static inline void e06_set_node_flags(E06Model *record, E06FlagsNode **nodes, u16 flags)
{
    u32 i;
    for (i = 0; i < record->count; i++) {
        if (nodes[i] != 0) nodes[i]->flags18 = flags;
    }
}
u32 func_00269470(u32 selector_arg, Func00269470State *state)
{
    Func00269470Fields *fields = &state->fields;
    u32 result = 0;

    switch (selector_arg & 0xFFFF) {
    case 1:
        if ((fields->flags_4A & 1) && fields->callback_70) fields->callback_70(1, fields, 0);
        func_00268798((struct E06State *)(&fields->work_4C));
        break;
    case 2:
        if (fields->flags_4A & 4) {
            func_00268AD8((struct E06State *)(&fields->work_4C), (u16)(fields->value_64), (s32)(fields->value_68), (u16)(fields->value_66), (s32)(fields->value_6C));
        } else func_00268800((struct E06State *)(&fields->work_4C));
        break;
    case 6:
        result = fields->work_4C.model->unk00;
        break;
    case 0: {
        /* The node flag is a halfword; widening this local changes the
         * compiler's Boolean conversion and the following loop allocation. */
        u16 mask;
        u16 initial_flags;
        /* This access view preserves the observed paired stores and reloads
         * during setup; ordinary accesses resume after initialization. */
        volatile Func00269470Fields *setup = fields;

        setup->flags_4A = g_func_00269470_flags;
        setup->callback_70 = g_func_00269470_callback;
        setup->value_64 = g_func_00269470_value_44;
        setup->value_66 = g_func_00269470_value_46;
        setup->value_68 = (s32)g_func_00269470_value_48;
        initial_flags = setup->flags_4A;
        fields->value_6C = (s32)g_func_00269470_value_4C;
        if (initial_flags & 4) {
            u16 value_66 = setup->value_66;
            void *value_3C = g_func_00269470_value_3C;
            u16 value_64 = setup->value_64;
            func_00268890((struct E06State *)(&fields->work_4C), (s32)(value_3C), (u16)(value_64), (s32)(fields->value_68), (u16)(value_66), (s32)(fields->value_6C));
        } else func_00268678((struct E06State *)(&fields->work_4C), (s32)(g_func_00269470_value_3C));

        mask = 0;
        if (fields->flags_4A & 0x10) mask |= 1;
        if (fields->flags_4A & 0x20) mask |= 2;
        e06_set_node_flags(fields->work_4C.model, (E06FlagsNode **)fields->work_4C.nodes, mask);
        func_00028C40(fields->matrix_00);
        fields->value_40.as_float = 1.0f;
        if ((fields->flags_4A & 1) && fields->callback_70) fields->callback_70(0, fields, 0);
        break;
    }
    case 5:
        func_00268358((s32)(fields->work_4C.model->count), (void **)(fields->work_4C.nodes));
        state->flags_08 &= ~4;
        break;
    case 3:
        func_00267FD0((s32)(state->value_10), (struct E06TransformInput *)(fields->work_4C.model), (struct E06TransformOutput *)(fields->work_4C.matrices));
        func_002682A4((float (*)[4])(fields->matrix_00), (struct E06HierarchyInput *)(fields->work_4C.model), (struct E06TransformOutput *)(fields->work_4C.matrices));
        func_00268400((s32)(fields->work_4C.model->count), (struct E06TransformOutput *)(fields->work_4C.matrices), (struct E06TripleNode **)(fields->work_4C.nodes));
        if (state->flags_0A & 0x1000) func_002677D0((s32)(fields->work_4C.model->count), (struct E06TripleNode **)(fields->work_4C.nodes), (s32)(state->value_0C));
        break;
    case 4:
        if (fields->flags_4A & 2) func_00268478((s32)(fields->work_4C.model->count), (struct E06TransformOutput *)(fields->work_4C.matrices), fields->value_40.as_float);
        if (fields->work_4C.unk10) {
            if (fields->flags_4A & 4) func_0026909C((s32)(state->value_10), (struct E06Model *)(fields->work_4C.model), (struct E06Node **)(fields->work_4C.nodes), (u16)(fields->value_66), (s32)(fields->value_6C));
            else func_00268F64((s32)(state->value_10), (struct E06Model *)(fields->work_4C.model), (struct E06Node **)(fields->work_4C.nodes));
        }
        if ((fields->flags_4A & 1) && fields->callback_70) fields->callback_70(2, fields, (void *)state->value_10);
        break;
    }
    return result;
}
