#include "game/effect_callbacks_p02.h"

typedef struct P02State2711DC P02State2711DC;
typedef struct P02Created271DBC {
    u8 unknown_00[0x3C];
    u32 value_3C, value_40;
    s32 value_44, value_48;
} P02Created271DBC;


void *func_00271DBC(s16 value, u32 first, u32 second, s32 ordinal)
{
    P02Created271DBC *created = func_00262AB0((void *)func_002711DC);

    created->value_3C = first;
    created->value_40 = second;
    created->value_44 = ordinal;
    created->value_48 = value;
    return created;
}
