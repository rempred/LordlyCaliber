typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))
#define RECORD_FIELD(record, type, offset) (*(type *)((u8 *)(record) + (offset)))
/* Selector byte at +0x33 and four-byte pointer entries share this bank.
 * Their observed addressing assigns no entry capacity. */
extern u8 D_800E7A90[];
extern RuntimeUnit *D_801F0CB0[];
extern u16 D_801F36D8[];

s32 func_00127D30(RuntimeUnit *unit)
{
    /* Keep the actual bank address calculation before the single list-base
       load. KMC schedules these genuine single-set inputs differently from
       the loop's walking pointer. */
    u32 selector = D_800E7A90[0x33];
    s32 field24 = UNIT_FIELD(s32, 0x24);
    u8 **entry = (u8 **)(D_800E7A90 + selector * 4);
    s32 *list = UNIT_FIELD(s32 *, 0xA8);
    u8 *lookup = *entry;
    u32 byte20 = UNIT_FIELD(u8, 0x20);

    f32 minimum;
    s32 best;
    s32 position;
    s32 index;

    if (field24 != byte20) {
        u32 offset = ((u32)field24 - 1) * 12;
        f32 dx = *(f32 *)((u32)unit + offset + 0x28) - UNIT_FIELD(f32, 0x08);
        f32 dz = *(f32 *)((u32)unit + offset + 0x30) - UNIT_FIELD(f32, 0x10);

        minimum = dx * dx + dz * dz;
    } else {
        minimum = 4050.0f;
    }

    best = -1;
    position = 0;

    /* The returned list position also indexes its actual candidate. A separate
       explicit walking pointer adds a move at the common loop entry in KMC. */
    for (index = list[position]; index != -1; index = list[position]) {
        RuntimeUnit *target = D_801F0CB0[index];

        if ((UNIT_FIELD(u32, 0x00) & 0x204) == 4 && UNIT_FIELD(s32, 0x80) == index) {
            if (UNIT_FIELD(u8, 0x20) == UNIT_FIELD(s32, 0x24)) {
                best = position;
                break;
            }
            goto next_record;
        }
        if ((RECORD_FIELD(target, u32, 0x00) & 0x31) == 0x31) {
            u32 column = lookup[RECORD_FIELD(target, s32, 0x14)];
            u16 *row = D_801F36D8 + (UNIT_FIELD(u32, 0x70) << 5);
            u16 value = row[column];

            if (value != 0xFFFF) {
                f32 dx = UNIT_FIELD(f32, 0x08) - RECORD_FIELD(target, f32, 0x08);
                f32 dz = UNIT_FIELD(f32, 0x10) - RECORD_FIELD(target, f32, 0x10);
                f32 distance = dx * dx + dz * dz;

                if (distance <= minimum) {
                    minimum = distance;
                    best = position;
                }
            }
        }
next_record:

        position++;
    }
    return best;
}
