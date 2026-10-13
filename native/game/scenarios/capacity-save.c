#include "save_format.h"
#include "capacity-save.h"

typedef unsigned char CapacityByte;
typedef unsigned int CapacityWord;

static CapacityWord capacity_bits(const CapacityByte *p, CapacityWord bit, CapacityWord count)
{
    CapacityWord value = 0;
    while (count--) {
        value = (value << 1) | ((p[bit >> 3] >> (7 - (bit & 7))) & 1);
        ++bit;
    }
    return value;
}
static void capacity_set_bit(CapacityByte *p, CapacityWord bit, CapacityWord value)
{
    CapacityWord mask = 1U << (7 - (bit & 7));
    p[bit >> 3] = (CapacityByte)((p[bit >> 3] & ~mask) | (value ? mask : 0));
}
static void capacity_move_bits(CapacityByte *p, CapacityWord to, CapacityWord from, CapacityWord count)
{
    while (count) {
        --count;
        capacity_set_bit(p, to + count, capacity_bits(p, from + count, 1));
    }
}
static void capacity_move_bytes(CapacityByte *p, CapacityWord to, CapacityWord from, CapacityWord count)
{
    while (count) { --count; p[to + count] = p[from + count]; }
}
static void capacity_zero(CapacityByte *p, CapacityWord count)
{
    while (count--) *p++ = 0;
}
int lc_save_expand_capacity(CapacityByte *record, CapacityWord capacity)
{
    CapacityWord i, j, id, cleared;
    CapacityByte *quick, *tags;
    CapacityByte padding;
    if (!record || capacity < LC_SAVE_CAPACITY_SUSPEND_BYTES) return 0;
    if (lc_save_record_valid(record, capacity, 15, 3)) return 1;
    if (!lc_save_record_valid(record, capacity, 15, 1)) return 0;
    cleared = !(record[0] | record[1] | record[2] | record[3]);
    quick = record + LC_SAVE_EXPANDED_NORMAL_BYTES;
    /* No legacy member may be silently reinterpreted as an added handle. */
    if (!cleared) for (i = 0; i < 20; ++i) for (j = 0; j < 5; ++j)
        if (capacity_bits(quick, 27300U + i * 137U + 14U + j * 8U, 8) >= 220U) return 0;

    /* Move every old mission byte, including unknown fields and padding, before
     * using its former storage for new character records and type metadata. */
    capacity_move_bytes(record, LC_CAPACITY_MISSION_OFFSET + 64U + 61U * 168U,
                        6256U + 3788U + 40U + 50U * 168U, 724);
    capacity_move_bytes(record, LC_CAPACITY_MISSION_OFFSET + 64U,
                        6256U + 3788U + 40U, 50U * 168U);
    capacity_move_bytes(record, LC_CAPACITY_MISSION_OFFSET, 6256U + 3788U, 40);
    capacity_zero(record + LC_CAPACITY_MISSION_OFFSET + 40U, 24);
    capacity_zero(record + LC_CAPACITY_MISSION_OFFSET + 64U + 50U * 168U, 11U * 168U);

    tags = record + LC_CAPACITY_TAG_OFFSET;
    capacity_zero(tags, LC_CAPACITY_TAG_BYTES);
    if (!cleared) {
        /* These offsets are in the original packed descriptors: ordinary class
         * at character bit112, and the120 Soldier HP entries at main bit41426.
         * Keep occupied legacy slots even when no squad currently refers to them.
         */
        for (i = 1; i < 100; ++i)
            if (capacity_bits(quick, i * 273U + 112U, 8)) tags[i] = 1;
        for (i = 0; i < 120; ++i)
            if (capacity_bits(record + 42, 41426U + i * 10U, 10)) tags[i + 100] = 2;
        for (i = 0; i < 20; ++i) for (j = 0; j < 5; ++j) {
            id = capacity_bits(quick, 27300U + i * 137U + 14U + j * 8U, 8);
            if (id) tags[id] = id < 100 ? 1 : 2;
        }
    }
    padding = quick[3787];
    capacity_move_bits(quick, 74135, 30040, 256);
    capacity_move_bits(quick, 69888, 27300, 2740);
    for (i = 27300; i < 69888; ++i) capacity_set_bit(quick, i, 0);
    for (i = 72628; i < 74135; ++i) capacity_set_bit(quick, i, 0);
    quick[9298] &= 0xFE;
    quick[9299] = padding;
    lc_save_record_seal(record, capacity, 15, 3);
    if (cleared) record[0] = record[1] = record[2] = record[3] = 0;
    return 1;
}
