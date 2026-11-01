#include "game/effect_model_e06.h"
#include "game/effect_dispatch_e07.h"

typedef struct Func0026A510Record {
    u8 unknown_00[0x30];
    float value_30;
    float value_34;
    float value_38;
    u8 unknown_3C[8];
} Func0026A510Record;

typedef struct Func0026A510Output {
    u32 unknown_00;
    s32 value_04;
    s32 value_08;
    s32 value_0C;
    u8 unknown_10[8];
    u16 flags_18;
    u16 unknown_1A;
} Func0026A510Output;

extern void func_0025F058(void *source, void *result);
extern void mtx_apply_vec3(float matrix[4][4], float x, float y, float z,
                           float *out_x, float *out_y, float *out_z);
extern float D_80220EE0[4][4];

static __inline__ void a6e4_translate(float first, float second, float third, s32 count,
                   Func0026A510Record *records, Func0026A510Output **slots)
{
    float matrix[4][4];
    s32 item;

    func_0025F058(D_80220EE0, matrix);
    matrix[3][0] = first;
    matrix[3][1] = second;
    matrix[3][2] = third;
    for (item = 0; item < count; item++) {
        if (slots[item]) {
            mtx_apply_vec3(matrix, records[item].value_30, records[item].value_34,
                          records[item].value_38, &first, &second, &third);
            slots[item]->value_04 = (s32)(first * 30.0f);
            slots[item]->value_08 = (s32)(second * 30.0f);
            slots[item]->value_0C = (s32)(third * 30.0f);
        }
    }
}


typedef struct Func0026A630Record {
    float matrix_00[4][4];
    u32 unknown_40;
} Func0026A630Record;

extern void func_0025F058(void *source, void *result);
extern void guMtxCatF(float left[4][4], float right[4][4], float result[4][4]);
extern float D_80220FF0[16];

static __inline__ void a6e4_combine(float first, float second, float third, s32 count,
                   Func0026A630Record *records, Func0026A510Output **slots)
{
    float matrix[4][4];
    s32 item;

    func_0025F058(D_80220FF0, matrix);
    matrix[3][0] = first;
    matrix[3][1] = second;
    matrix[3][2] = third;
    for (item = 0; item < count; item++) {
        if (slots[item]) {
            guMtxCatF(records[item].matrix_00, matrix, records[item].matrix_00);
        }
    }
}

typedef void (*Func0026A6E4Callback)(u32 reason, void *fields, void *argument);

typedef union Func0026A6E4Word {
    float as_float;
    u32 as_u32;
} Func0026A6E4Word;

typedef struct Func0026A6E4Fields {
    float value_00, value_04, value_08;
    float value_0C, value_10, value_14;
    Func0026A6E4Word value_18;
    u16 flags_1C;
    u16 unknown_1E;
    u32 work_20;
    u32 *record_24;
    void *records_28;
    Func0026A510Output **slots_2C;
    u16 value_30;
    u8 unknown_32[6];
    u16 value_38;
    u16 value_3A;
    void *value_3C;
    void *value_40;
    Func0026A6E4Callback callback_44;
} Func0026A6E4Fields;

typedef struct Func0026A6E4State {
    u8 unknown_00[8];
    u16 flags_08, flags_0A;
    void *value_0C, *value_10;
    u8 unknown_14[8];
    Func0026A6E4Fields fields_1C;
} Func0026A6E4State;

extern volatile u16 g_func_00269470_flags;
extern volatile Func0026A6E4Callback g_func_00269470_callback;
extern volatile u16 g_func_00269470_value_44;
extern volatile u16 g_func_00269470_value_46;
extern void * volatile g_func_00269470_value_48;
extern void * volatile g_func_00269470_value_4C;
extern void * volatile g_func_00269470_value_3C;
extern void func_00028F60(void *matrix, float first, float second, float third);
u32 func_0026a6e4(u16 command, Func0026A6E4State *state)
{
    u32 result = 0;
    Func0026A6E4Fields *fields = &state->fields_1C;

    switch (command) {
    case 1:
        if ((fields->flags_1C & 1) && fields->callback_44) {
            fields->callback_44(1, fields, 0);
        }
        func_00268798((struct E06State *)(&fields->work_20));
        break;
    case 2:
        if (fields->flags_1C & 4) {
            func_00268AD8((struct E06State *)(&fields->work_20), (u16)(fields->value_38), (s32)(fields->value_3C),
                          (u16)(fields->value_3A), (s32)(fields->value_40));
        } else {
            func_00268800((struct E06State *)(&fields->work_20));
        }
        break;
    case 6:
        result = fields->record_24[0];
        break;
    case 0: {
        /* This value is stored as a halfword in each selected output header. */
        u16 flags;
        u32 item;
        u32 *record;
        Func0026A510Output **slots;
        volatile Func0026A6E4Fields *init_fields;

        /* Confine the observed setup access order to these six assignments. */
        init_fields = fields;
        init_fields->flags_1C = g_func_00269470_flags;
        init_fields->callback_44 = g_func_00269470_callback;
        init_fields->value_38 = g_func_00269470_value_44;
        init_fields->value_3A = g_func_00269470_value_46;
        init_fields->value_3C = g_func_00269470_value_48;
        init_fields->value_40 = g_func_00269470_value_4C;
        if (fields->flags_1C & 4) {
            /* Retain the reference's two halfword reads around this global read. */
            u16 value_3A = init_fields->value_3A;
            void *value = g_func_00269470_value_3C;
            u16 value_38 = init_fields->value_38;

            func_00268890((struct E06State *)(&fields->work_20), (s32)(value), (u16)(value_38), (s32)(fields->value_3C),
                          (u16)(value_3A), (s32)(fields->value_40));
        } else {
            func_00268678((struct E06State *)(&fields->work_20), (s32)(g_func_00269470_value_3C));
        }
        flags = 0;
        if (fields->flags_1C & 0x10) {
            flags |= 1;
        }
        if (fields->flags_1C & 0x20) {
            flags |= 2;
        }
        record = fields->record_24;
        slots = fields->slots_2C;
        for (item = 0; item < record[1]; item++, slots++) {
            if (*slots) {
                (*slots)->flags_18 = flags;
            }
        }
        fields->value_00 = fields->value_04 = fields->value_08 = 0.0f;
        fields->value_18.as_float = fields->value_0C = fields->value_10 =
            fields->value_14 = 1.0f;
        if ((fields->flags_1C & 1) && fields->callback_44) {
            fields->callback_44(0, fields, 0);
        }
        break;
    }
    case 5:
        func_00268358((s32)(fields->record_24[1]), (void **)(fields->slots_2C));
        state->flags_08 &= ~4;
        break;
    case 3: {
        float matrix[4][4];

        func_00267FD0((s32)(state->value_10), (struct E06TransformInput *)(fields->record_24), (struct E06TransformOutput *)(fields->records_28));
        func_00028F60(matrix, fields->value_0C, fields->value_10, fields->value_14);
        func_002682A4((float (*)[4])(matrix), (struct E06HierarchyInput *)(fields->record_24), (struct E06TransformOutput *)(fields->records_28));
        a6e4_translate(fields->value_00, fields->value_04, fields->value_08,
                        fields->record_24[1], fields->records_28, fields->slots_2C);
        if (state->flags_0A & 0x1000) {
            func_002677D0((s32)(fields->record_24[1]), (struct E06TripleNode **)(fields->slots_2C), (s32)(state->value_0C));
        }
        a6e4_combine(fields->value_00, fields->value_04, fields->value_08,
                      fields->record_24[1], fields->records_28, fields->slots_2C);
        break;
    }
    case 4:
        if (fields->flags_1C & 2) {
            func_00268478((s32)(fields->record_24[1]), (struct E06TransformOutput *)(fields->records_28),
                          fields->value_18.as_float);
        }
        if (fields->value_30) {
            if (fields->flags_1C & 4) {
                func_0026909C((s32)(state->value_10), (struct E06Model *)(fields->record_24), (struct E06Node **)(fields->slots_2C),
                              (u16)(fields->value_3A), (s32)(fields->value_40));
            } else {
                func_00268F64((s32)(state->value_10), (struct E06Model *)(fields->record_24), (struct E06Node **)(fields->slots_2C));
            }
        }
        if ((fields->flags_1C & 1) && fields->callback_44) {
            fields->callback_44(2, fields, state->value_10);
        }
        break;
    }
    return result;
}