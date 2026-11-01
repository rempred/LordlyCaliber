#include "game/effect_presentation_p06_internal.h"

typedef struct Cd84Header {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
} Cd84Header;

void func_0024CD84(Cd84Header *object, u32 flags)
{
    object->flags_0A |= flags & 0xD000;
    if (flags & 0x1000) {
        s16 selected;

        if (flags & 0x400) {
            selected = (flags & 0x800) ? 10000 : -10000;
        } else {
            selected = (flags & 0x800) ? 1000 : -1000;
        }
        object->value_0C = selected;
    }
}
