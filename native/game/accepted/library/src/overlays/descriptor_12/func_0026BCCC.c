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

void func_0026BCCC(Func0026B820Fields *fields, s32 item)
{
    if (!(fields->flags_08[item] & 1)) {
        Func0026B820Record *record = &fields->records_0C[item];

        func_00268358((s32)(record->entry_04[1]), (void **)(record->slots_0C));
        fields->flags_08[item] |= 1;
    }
}
