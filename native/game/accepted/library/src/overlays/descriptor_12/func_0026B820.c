#include "game/effect_model_e06.h"
#include "game/effect_dispatch_e07.h"

typedef struct Func0026B820Values {
    float value_00;
    float value_04;
    float value_08;
    float value_0C;
    float value_10;
    float value_14;
} Func0026B820Values;

typedef struct Func0026B820Record {
    u32 unknown_00;
    u32 *entry_04;
    void *records_08;
    void *slots_0C;
    u16 value_10;
    u8 unknown_12[6];
} Func0026B820Record;

typedef struct Func0026B820Fields {
    Func0026B820Values *values_00;
    s32 count_04;
    u16 *flags_08;
    Func0026B820Record *records_0C;
} Func0026B820Fields;

typedef struct Func0026B820State {
    u8 unknown_00[8];
    u16 flags_08;
    u16 flags_0A;
    void *value_0C;
    void *value_10;
    u8 unknown_14[8];
    Func0026B820Fields fields_1C;
} Func0026B820State;

struct Func0026A510Record;
struct Func0026A510Output;
struct Func0026A630Record;
extern volatile u32 g_func_0026B820_value_38;
extern volatile u32 g_func_0026B820_count;
extern void *func_00001330(u32 bytes);
extern void func_000016C4(void *allocation);
extern void func_00028F60(float matrix[4][4], float first, float second, float third);
u32 func_0026B820(u16 command, Func0026B820State *state)
{
    u32 result = 0;
    Func0026B820Fields *fields = &state->fields_1C;
    float matrix[4][4];
    s32 item;

    switch (command) {
    case 1:
        for (item = 0; item < fields->count_04; item++) {
            func_00268798((struct E06State *)(&fields->records_0C[item]));
        }
        break;

    case 2:
        for (item = 0; item < fields->count_04; item++) {
            func_00268800((struct E06State *)(&fields->records_0C[item]));
        }
        func_000016C4(fields->values_00);
        func_000016C4(fields->flags_08);
        func_000016C4(fields->records_0C);
        break;

    case 6:
        result = fields->records_0C[0].entry_04[0];
        break;

    case 0: {
        s32 init_item;

        fields->count_04 = g_func_0026B820_count;
        fields->values_00 = func_00001330(fields->count_04 * sizeof(*fields->values_00));
        fields->flags_08 = func_00001330(fields->count_04 * sizeof(*fields->flags_08));
        fields->records_0C = func_00001330(fields->count_04 * sizeof(*fields->records_0C));
        for (init_item = 0; init_item < fields->count_04; init_item++) {
            Func0026B820Values *values;

            func_00268678((struct E06State *)(&fields->records_0C[init_item]), (s32)((void *)g_func_0026B820_value_38));
            fields->flags_08[init_item] = 1;
            /* The six writes use the same selected values record. */
            values = &fields->values_00[init_item];
            values->value_00 = values->value_04 = values->value_08 = 0.0f;
            values->value_0C = values->value_10 = values->value_14 = 1.0f;
        }
        break;
    }

    case 5:
        for (item = 0; item < fields->count_04; item++) {
            u16 flags = fields->flags_08[item];

            if ((flags & 1) && !(flags & 0x8000)) {
                func_00268358((s32)(fields->records_0C[item].entry_04[1]),
                              (void **)(fields->records_0C[item].slots_0C));
                fields->flags_08[item] |= 0x8000;
            }
        }
        state->flags_08 &= ~4;
        break;

    case 3:
        for (item = 0; item < fields->count_04; item++) {
            if (fields->flags_08[item] & 1) {
                func_00267FD0((s32)(state->value_10), (struct E06TransformInput *)(fields->records_0C[item].entry_04),
                              (struct E06TransformOutput *)(fields->records_0C[item].records_08));
            }
        }
        for (item = 0; item < fields->count_04; item++) {
            if (fields->flags_08[item] & 1) {
                func_00028F60(matrix, fields->values_00[item].value_0C,
                              fields->values_00[item].value_10,
                              fields->values_00[item].value_14);
                func_002682A4((float (*)[4])(matrix), (struct E06HierarchyInput *)(fields->records_0C[item].entry_04),
                              (struct E06TransformOutput *)(fields->records_0C[item].records_08));
            }
        }
        for (item = 0; item < fields->count_04; item++) {
            if (fields->flags_08[item] & 1) {
                func_0026a510(fields->values_00[item].value_00,
                              fields->values_00[item].value_04,
                              fields->values_00[item].value_08,
                              fields->records_0C[item].entry_04[1],
                              (struct Func0026A510Record *)fields->records_0C[item].records_08,
                              (struct Func0026A510Output **)fields->records_0C[item].slots_0C);
            }
        }
        for (item = 0; item < fields->count_04; item++) {
            if (fields->flags_08[item] & 1) {
                func_0026a630(fields->values_00[item].value_00,
                              fields->values_00[item].value_04,
                              fields->values_00[item].value_08,
                              fields->records_0C[item].entry_04[1],
                              (struct Func0026A630Record *)fields->records_0C[item].records_08,
                              (void **)fields->records_0C[item].slots_0C);
            }
        }
        break;

    case 4:
        for (item = 0; item < fields->count_04; item++) {
            if ((fields->flags_08[item] & 1) && fields->records_0C[item].value_10) {
                func_00268F64((s32)(state->value_10), (struct E06Model *)(fields->records_0C[item].entry_04),
                              (struct E06Node **)(fields->records_0C[item].slots_0C));
            }
        }
        break;
    }
    return result;
}
