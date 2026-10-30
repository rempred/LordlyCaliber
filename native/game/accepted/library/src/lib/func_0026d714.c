#include "game/effect_callbacks_p02.h"

typedef struct P02ChildState P02ChildState;

typedef struct P02CreatedD714 {
    u8 unknown_00[0x48];
    u32 value_48;
    u32 value_4C;
    s32 value_50;
} P02CreatedD714;


void * func_0026d714(s16 value, u32 first, u32 second, s32 unusedOrdinal)
{
    P02CreatedD714 *created = func_00262AB0((void *)func_0026ce74);

    created->value_48 = first;
    created->value_4C = second;
    created->value_50 = value;
    /* F578 supplies this fourth argument; this creator does not consume it. */
    (void)unusedOrdinal;
    return created;
}
