#include "game/combat_pose_pool.h"

extern int func_002015C8(int, int, int, int);
extern int func_0020161C(int, int, int, int);
extern int func_0020156C(int);
extern int func_000454E0(int);
extern CombatPosePoolRecord *func_00205C88(CombatPosePoolRecord *, int, int, int, int, int);

static __inline__ int resolve_variant(int a, int variant)
{
    switch (a) {
    case 0x3B: return 11;
    case 0x3C: return 13;
    case 0x3D: return 15;
    case 0x3E: return 18;
    case 0x39:
    case 0x3F:
    case 0x40: return 2;
    case 0x41: return 3;
    case 0x42: return 4;
    case 0x43: return 5;
    case 0x44: return 6;
    case 0x3A:
    case 0x46: return 8;
    }
    return variant;
}

static __inline__ CombatPosePoolRecord *find_0C(int a, int b, int c, int d)
{
    int handle = func_002015C8(a, b, c, d);
    int selected = func_0020161C(a, b, c, d);
    CombatPosePoolRecord *record;
    u32 i;
    for (i = 0, record = D_801D0728; i < 20; i++, record++) {
        if (record->field_00 == handle && record->field_0C == selected)
            return record;
    }
    return 0;
}

#define DEFINE_VARIANT_LOOKUP(name, field, map) \
static __inline__ CombatPosePoolRecord *name(int a, int b, int c, int d, int variant) \
{ \
    int handle; \
    u32 selected, i; \
    CombatPosePoolRecord *record; \
    variant = resolve_variant(a, variant); \
    handle = func_002015C8(a, b, c, d); \
    selected = (u8)map((u16)variant); \
    for (i = 0, record = D_801D0728; i < 20; i++, record++) { \
        if (record->field_00 == handle && record->field == selected) \
            return record; \
    } \
    return 0; \
}

DEFINE_VARIANT_LOOKUP(find_10, field_10, func_0020156C)
DEFINE_VARIANT_LOOKUP(find_14, field_14, func_000454E0)

void func_00207658(int *a, int *b, int *d, int *c, int *variants, u32 count)
{
    CombatPosePoolRecord *found[3][10];
    u32 i;
    CombatPosePoolRecord *record;
    int remaining, repeat;

    for (i = 0; i < count; i++) {
        variants[i] = resolve_variant(a[i], variants[i]);
        found[0][i] = find_0C(a[i], b[i], c[i], d[i]);
        found[1][i] = find_10(a[i], b[i], c[i], d[i], variants[i]);
        found[2][i] = find_14(a[i], b[i], c[i], d[i], variants[i]);
        if (found[0][i] == 0 || found[1][i] == 0 || found[2][i] == 0) {
            /* Keep the old countdown value: the retail scan tests it before
             * decrementing. The shared exit prevents KMC from peeling the scan. */
            record = D_801D0728;
            remaining = 19;
            do {
                if (record->field_00 == 0)
                    goto found_slot;
                record++;
                repeat = remaining;
                remaining--;
            } while (repeat);
found_slot:
            /* Retail waits here if all twenty records were occupied. */
            do {
            } while (remaining < 0);
            record = func_00205C88(record, a[i], b[i], c[i], d[i], variants[i]);
            if (found[0][i] == 0)
                found[0][i] = record;
            if (found[1][i] == 0)
                found[1][i] = record;
            if (found[2][i] == 0)
                found[2][i] = record;
        }
    }
}

#undef DEFINE_VARIANT_LOOKUP
