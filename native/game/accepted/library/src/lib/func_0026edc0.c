#include "game/effect_callbacks_p02.h"

typedef struct P02StateE64C P02StateE64C;

typedef struct P02CreatedEDC0 {
    u8 unknown_00[0x3C];
    u32 value_3C;
    u32 value_40;
    s32 value_44;
} P02CreatedEDC0;


void * func_0026edc0(s16 value, u32 first, u32 second, s32 unusedOrdinal)
{
    P02CreatedEDC0 *created = func_00262AB0((void *)func_0026e64c);

    created->value_3C = first;
    created->value_40 = second;
    created->value_44 = value;
    /* F578 supplies this fourth argument; this creator does not consume it. */
    (void)unusedOrdinal;
    return created;
}
