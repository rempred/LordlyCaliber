#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

typedef struct P05FlagObject {
    u8 unknown_00[0xA];
    u16 flags_0A;
} P05FlagObject;


void func_0024E408(u16 value)
{
    P05FlagObject *object = func_0026C070(value);

    object->flags_0A |= 0x8000;
}
