#include "game/effect_adapters_e08.h"

typedef struct Cdd8InputRecord {
    u16 value_00;
    u8 value_02;
    u8 unknown_03;
    u16 flags_04;
} Cdd8InputRecord;

typedef struct Cdd8Header {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
} Cdd8Header;

typedef void *(*Cdd8Callback)(s32 argument);
extern Cdd8Callback g_func_0024CDD8_callbacks[];

void *func_0024CDD8(void *recordView)
{
    Cdd8InputRecord *record = (Cdd8InputRecord *)recordView;
    Cdd8Header *object;
    u16 flags;

    object = g_func_0024CDD8_callbacks[record->value_02 & 3](record->value_00);
    flags = record->flags_04;
    object->flags_0A |= flags & 0xD000;
    if (flags & 0x1000) {
        /* The word store receives a signed-halfword intermediate. */
        s16 selected;

        if (flags & 0x400) {
            selected = (flags & 0x800) ? 10000 : -10000;
        } else {
            selected = (flags & 0x800) ? 1000 : -1000;
        }
        object->value_0C = selected;
    }
    return (u8 *)object + 0x1C;
}
