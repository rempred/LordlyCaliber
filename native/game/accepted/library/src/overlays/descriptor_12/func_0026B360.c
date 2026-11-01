#include "game/effect_model_e06.h"
#include "game/effect_dispatch_e07.h"

typedef struct Func0026B360Record {
    u32 unknown_00;
    u32 *entry_04;
    void *records_08;
    void *slots_0C;
    u16 value_10;
    u8 unknown_12[6];
} Func0026B360Record;

typedef struct Func0026B360Fields {
    float (*matrices_00)[4][4];
    s32 count_04;
    u16 *flags_08;
    Func0026B360Record *records_0C;
} Func0026B360Fields;

typedef struct Func0026B360State {
    u8 unknown_00[8];
    u16 flags_08;
    u16 flags_0A;
    void *value_0C;
    void *value_10;
    u8 unknown_14[8];
    Func0026B360Fields fields_1C;
} Func0026B360State;

extern volatile u32 g_func_0026B360_value_38;
extern volatile u32 g_func_0026B360_count;
extern void *func_00001330(u32 bytes);
extern void func_000016C4(void *allocation);
extern void guMtxIdentF(float matrix[4][4]);
struct Func0026A078Input;
struct Func0026A078Record;


u32 func_0026B360(u16 command, Func0026B360State *state)
{
    u32 result = 0;
    Func0026B360Fields *fields = &state->fields_1C;
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
        func_000016C4(fields->matrices_00);
        func_000016C4(fields->flags_08);
        func_000016C4(fields->records_0C);
        break;

    case 6:
        result = fields->records_0C[0].entry_04[0];
        break;

    case 0:
        fields->count_04 = g_func_0026B360_count;
        fields->matrices_00 = func_00001330(fields->count_04 * sizeof(*fields->matrices_00));
        fields->flags_08 = func_00001330(fields->count_04 * sizeof(*fields->flags_08));
        fields->records_0C = func_00001330(fields->count_04 * sizeof(*fields->records_0C));
        for (item = 0; item < fields->count_04; item++) {
            func_00268678((struct E06State *)(&fields->records_0C[item]), (s32)((void *)g_func_0026B360_value_38));
            fields->flags_08[item] = 1;
            guMtxIdentF(fields->matrices_00[item]);
        }
        break;

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
                func_002682A4((float (*)[4])(fields->matrices_00[item]), (struct E06HierarchyInput *)(fields->records_0C[item].entry_04),
                              (struct E06TransformOutput *)(fields->records_0C[item].records_08));
            }
        }
        for (item = 0; item < fields->count_04; item++) {
            if (fields->flags_08[item] & 1) {
                func_00268400((s32)(fields->records_0C[item].entry_04[1]),
                              (struct E06TransformOutput *)(fields->records_0C[item].records_08),
                              (struct E06TripleNode **)(fields->records_0C[item].slots_0C));
            }
        }
        for (item = 0; item < fields->count_04; item++) {
            if ((fields->flags_08[item] & 1) && (state->flags_0A & 0x1000)) {
                func_002677D0((s32)(fields->records_0C[item].entry_04[1]),
                              (struct E06TripleNode **)(fields->records_0C[item].slots_0C), (s32)(state->value_0C));
            }
        }
        for (item = 0; item < fields->count_04; item++) {
            if (fields->flags_08[item] & 1) {
                func_0026a078((s32)state->value_10,
                              (struct Func0026A078Input *)fields->records_0C[item].entry_04,
                              (struct Func0026A078Record *)fields->records_0C[item].records_08);
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
