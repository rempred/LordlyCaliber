#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"


typedef struct P05FactoryNode {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
    u8 unknown_10[0xC];
} P05FactoryNode;

struct P01PlacementObject;
/* Descriptor 11 maps this dispatch subtable to original ROM 0x25E2F4. */

struct P01PlacementObject *func_0024CF10(P05FactoryRecord *record)
{
    P05FactoryNode *node = D_80209734[record->selector_02 & 3](record->value_00);
    u16 flags = record->flags_04;

    node->flags_0A |= flags & 0xD000;
    if (flags & 0x1000) {
        s16 value;

        if (flags & 0x400) {
            value = flags & 0x800 ? 10000 : -10000;
        } else {
            value = flags & 0x800 ? 1000 : -1000;
        }
        node->value_0C = value;
    }
    return (struct P01PlacementObject *)((u8 *)node + 0x1C);
}
