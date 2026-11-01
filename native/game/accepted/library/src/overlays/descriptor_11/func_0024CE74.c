#include "game/effect_presentation_p06_internal.h"

typedef struct Ce74InputRecord {
    u16 value_00;
    u8 value_02;
    u8 unknown_03;
    u16 flags_04;
} Ce74InputRecord;

typedef struct Ce74Header {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
} Ce74Header;

extern void *(*p06_data_80209728[])(s32 argument);

void *func_0024CE74(Ce74InputRecord *record)
{
    Ce74Header *object;
    u16 flags;

    object = p06_data_80209728[record->value_02 & 3](record->value_00);
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
