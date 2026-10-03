#include "save_format.h"
typedef unsigned char u8;
typedef unsigned int u32;

static u32 get32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}
static void put32(u8 *p, u32 value)
{
    p[0] = (u8)(value >> 24); p[1] = (u8)(value >> 16); p[2] = (u8)(value >> 8); p[3] = (u8)value;
}
u32 lc_save_record_offset(u32 slot, u32 expanded)
{
    if (expanded > 1) return ~0U;
    if (slot == 14) return 0;
    if (slot == 15) return expanded ? 0x30F0U : 0x30B0U;
    if (slot < 2) return 16U + slot * (expanded ? LC_SAVE_EXPANDED_NORMAL_BYTES : LC_SAVE_NORMAL_BYTES);
    return ~0U;
}
u32 lc_save_record_size(u32 slot, u32 expanded)
{
    if (expanded > 1) return 0;
    if (slot == 14) return 16;
    if (slot == 15) return expanded ? LC_SAVE_EXPANDED_SUSPEND_BYTES : LC_SAVE_SUSPEND_BYTES;
    if (slot < 2) return expanded ? LC_SAVE_EXPANDED_NORMAL_BYTES : LC_SAVE_NORMAL_BYTES;
    return 0;
}
/* Sum and popcount are independent 16-bit accumulators seeded with the record's
 * physical SRAM offset, as in the original 80075A84/80075B00 functions.
 */
static u32 checksums(const u8 *p, u32 length, u32 offset)
{
    u32 sum = offset, count = offset, i, value, bit;
    for (i = 12; i < length; ++i) {
        value = p[i]; sum += value;
        for (bit = 0; bit < 8; ++bit) { count += value & 1U; value >>= 1; }
    }
    return (sum << 16) | (count & 65535U);
}
int lc_save_record_seal(u8 *record, u32 capacity, u32 slot, u32 expanded)
{
    u32 length = lc_save_record_size(slot, expanded);
    if (!record || !length || capacity < length) return 0;
    put32(record, checksums(record, length, lc_save_record_offset(slot, expanded)));
    put32(record + 4, expanded && slot != 14 ? 0x4C433035U : 0x51756573U);
    put32(record + 8, expanded && slot != 14 ? 0x4F473321U : 0x744F4733U);
    return 1;
}
int lc_save_record_valid(const u8 *record, u32 capacity, u32 slot, u32 expanded)
{
    u32 length = lc_save_record_size(slot, expanded);
    if (!record || !length || capacity < length) return 0;
    if (get32(record + 4) != (expanded && slot != 14 ? 0x4C433035U : 0x51756573U) ||
        get32(record + 8) != (expanded && slot != 14 ? 0x4F473321U : 0x744F4733U)) return 0;
    /* The game uses a signed but zero-checksum suspend record as 'no suspend'. */
    if (slot == 15 && get32(record) == 0) return 1;
    return get32(record) == checksums(record, length, lc_save_record_offset(slot, expanded));
}
int lc_save_expand_record(u8 *record, u32 capacity, u32 slot)
{
    u32 oldLength = lc_save_record_size(slot, 0), length = lc_save_record_size(slot, 1);
    u32 i, byte, value, cleared;
    if (capacity < length || !lc_save_record_valid(record, capacity, slot, 0)) return 0;
    if (slot == 14) return 1;
    cleared = slot == 15 && get32(record) == 0;
    /* Move the suspend-only payload backward to permit in-place conversion. */
    for (i = oldLength; i > LC_SAVE_NORMAL_BYTES; --i)
        record[i - 1 + LC_SAVE_DISCOVERY_BYTES] = record[i - 1];
    for (i = 0; i < LC_SAVE_DISCOVERY_BYTES; ++i)
        record[LC_SAVE_NORMAL_BYTES + i] = 0;
    /* Main payload begins at record +0x2A. The original descriptor places the
     * eleven discovery bytes at bit 43196, independently checked by executing
     * the ROM's loader/codec. This is not an aligned byte slice.
     */
    if (!cleared) for (i = 0; i < 11; ++i) {
        byte = 0x2AU + (LC_SAVE_LEGACY_DISCOVERY_BIT >> 3) + i;
        value = ((u32)record[byte] << 8) | record[byte + 1];
        record[LC_SAVE_NORMAL_BYTES + i] = (u8)(value >> (8 - (LC_SAVE_LEGACY_DISCOVERY_BIT & 7)));
    }
    lc_save_record_seal(record, capacity, slot, 1);
    if (cleared) put32(record, 0);
    return 1;
}
