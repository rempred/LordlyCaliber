#include "game/effect_adapters_e08.h"

typedef struct Cd40PointerFields {
    u8 unknown_00[0x38];
    float *value_38;
    float *value_3C;
    float *value_40;
} Cd40PointerFields;

extern s32 resource_free(u8 *pointer);

void func_0024CD40(void *stateView)
{
    Cd40PointerFields *fields = (Cd40PointerFields *)stateView;

    resource_free((u8 *)fields->value_38);
    fields->value_38 = 0;
    resource_free((u8 *)fields->value_3C);
    fields->value_3C = 0;
    resource_free((u8 *)fields->value_40);
    fields->value_40 = 0;
}
