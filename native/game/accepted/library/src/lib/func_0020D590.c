#include "game/combat_types.h"

typedef struct SupplementBoundsRecord {
    unsigned char field_00[0x40];
    u32 flags;
    int field_44, field_48, field_4C, resource;
} SupplementBoundsRecord;

typedef struct CombatPoseBounds {
    int low04, low08, high04, high08;
} CombatPoseBounds;

extern u32 func_00204F34(int, int, int, int, u32, u32,
                       unsigned char *, unsigned char *, unsigned char *);
extern CombatPoseBounds func_00205378(int, int);

static __inline__ int flag(SupplementBoundsRecord *record, int bit)
{
    if (!record) return 0;
    return (record->flags >> bit) & 1;
}

int func_0020D590(SupplementBoundsRecord *record)
{
    CombatPoseBounds bounds;
    unsigned char output;
    u32 index = 0;
    u32 result;
    if (record->field_48 == 0x101) return 0x26;
    for (;;) {
        result = func_00204F34(record->field_48, record->field_4C,
                              flag(record, 10), flag(record, 8),
                              0, index, &output, 0, 0);
        if ((unsigned char)result == 1) break;
        index++;
    }
    bounds = func_00205378(record->resource, output);
    return (int)(0u - (u32)bounds.low08);
}
