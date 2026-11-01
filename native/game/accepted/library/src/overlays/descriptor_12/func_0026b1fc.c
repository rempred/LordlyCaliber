#include "game/effect_model_e06.h"
#include "game/effect_dispatch_e07.h"

typedef struct Func0026ADD0Record {
    u32 unknown_00;
    u32 *entry_04;
    void *records_08;
    void *slots_0C;
    u16 value_10;
    u8 unknown_12[6];
} Func0026ADD0Record;

typedef struct Func0026ADD0Fields {
    float (*matrices_00)[4][4];
    s32 count_04;
    u16 *flags_08;
    Func0026ADD0Record *records_0C;
} Func0026ADD0Fields;

void func_0026b1fc(Func0026ADD0Fields *fields, s32 item)
{
    if (!(fields->flags_08[item] & 1)) {
        Func0026ADD0Record *record = &fields->records_0C[item];

        func_00268358((s32)(record->entry_04[1]), (void **)(record->slots_0C));
        fields->flags_08[item] |= 1;
    }
}
