#include "common/types.h"

typedef struct P01Record {
    u16 value_00;
    u16 unknown_02;
    u16 flags_04;
} P01Record;

typedef struct P01Node {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
    u8 unknown_10[0xC];
} P01Node;

struct P01PlacementObject;
extern void *p01_table_8020A170[];
extern P01Node *func_002698A0(u32, void *);

struct P01PlacementObject *func_0024CFAC(void *argument, s32 index)
{
    P01Record *record = argument;
    P01Node *node = func_002698A0(record->value_00, p01_table_8020A170[index]);
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
