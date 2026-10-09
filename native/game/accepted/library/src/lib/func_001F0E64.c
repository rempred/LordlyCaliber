#include "game/combat_types.h"

typedef struct Func001F0E64Record {
    int field_00;
    u16 field_04;
    short field_06;
    u16 field_08, field_0A;
    u16 field_0C, field_0E, field_10;
    unsigned char field_12[16];
    unsigned char field_22[16];
    unsigned char field_32[20];
    unsigned char field_46, field_47, field_48;
} Func001F0E64Record;

extern int rand(void);
extern void func_8009C970(void *, int, unsigned int);
extern void func_80093380(void *, unsigned int);

/* Keeping the unsigned remainder in this inline result gives the caller the
 * original selector lifetime. Returning the combined word instead changes
 * the entry copy and predicate register with the pinned compiler. */
static __inline__ u32 random_selector_value(void)
{
    u32 first, second, third;
    first = rand();
    second = rand();
    third = rand();
    first <<= 18;
    first &= 0x0C000000u;
    second <<= 15;
    first |= second;
    first |= third;
    return first % 5u;
}

void func_001F0E64(Func001F0E64Record *record, u32 incomingSelector)
{
    u32 selector = incomingSelector;

    /* Only the sentinel test narrows the incoming selector. */
    if ((unsigned char)selector == 0xFF) return;
    if (selector == 0 && (record->field_48 & 2)) {
        selector = random_selector_value();
    }
    if (record->field_48 & 1) selector += 50;
    record->field_04 = selector;
    record->field_06 = -1;
    record->field_08 = 0;
    record->field_10 = 0;
    record->field_0E = 0;
    record->field_0C = 0;
    record->field_46 = 0;
    record->field_47 = 0;
    func_8009C970(record->field_12, 0xFF, 16);
    func_80093380(record->field_22, 16);
}
