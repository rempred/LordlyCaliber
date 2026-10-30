#include "game/effect_callbacks_p02.h"

typedef struct P02State271E5C P02State271E5C;
typedef struct P02Created27271C {
    u8 unknown_00[0x30];
    u32 value_30, value_34;
    s32 value_38, value_3C;
} P02Created27271C;


void *func_0027271C(s16 value, u32 first, u32 second, s32 ordinal)
{
    P02Created27271C *created = func_00262AB0((void *)func_00271E5C);

    created->value_30 = first;
    created->value_34 = second;
    created->value_38 = ordinal;
    created->value_3C = value;
    return created;
}
