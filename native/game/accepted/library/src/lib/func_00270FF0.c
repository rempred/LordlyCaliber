#include "game/effect_callbacks_p02.h"

typedef struct P02State2705EC P02State2705EC;
typedef struct P02Created70FF0 {
    u8 unknown_00[0x3C];
    u32 value_3C, value_40;
    s32 value_44, value_48;
} P02Created70FF0;

extern s16 func_0024BCE8(u8);

/* This creator consumes all four arguments supplied by F578. Its accepted
 * physical body includes both sides of the original 0x271000 file split. */
void *func_00270FF0(s16 value, u32 first, u32 second, s32 ordinal)
{
    P02Created70FF0 *created = func_00262AB0((void *)func_002705EC);

    created->value_3C = first;
    created->value_40 = second;
    created->value_44 = ordinal;
    created->value_48 = value;
    func_0024BCE8((u8)first);
    return created;
}
