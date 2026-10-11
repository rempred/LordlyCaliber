#include "save_format.h"
#include "save_checksums.h"
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
/* Accepted func_00005D9C signs the payload after the twelve-byte header.
 * Sizes and seeds remain configurable for the editor's expanded discovery data.
 */
static u32 checksums(const u8 *p, u32 length, u32 offset)
{
    return (lc_save_byte_sum(p + 12, length - 12, offset) << 16) |
           lc_save_bit_sum(p + 12, length - 12, offset);
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
    /* Accepted 0000581C/00005978/00005A88: check the signature before the two
     * checksum lanes. SRAM cache/DMA is caller-owned; PC fields are big-endian.
     */
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
static u32 marker_crc(const u8 *data)
{
    u32 crc = ~0U, i, bit;
    for (i = 0; i < 12; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
int lc_save_image_version(const u8 *image, u32 capacity)
{
    const u8 *marker, *record;
    u32 i, slot;
    if (!image || capacity < LC_SAVE_IMAGE_BYTES) return -1;
    marker = image + LC_SAVE_IMAGE_MARKER;
    if (get32(marker) == 0x4C433035U) {
        if (get32(marker + 4) != 0x53524131U || get32(marker + 8) != 0x00010020U ||
            get32(marker + 12) != marker_crc(marker)) return -1;
        return 1;
    }
    /* A new record without its layout marker may be a partial write. Do not
     * interpret it as legacy merely because the marker itself is damaged.
     */
    for (i = 0; i < 3; ++i) {
        slot = i == 2 ? 15 : i;
        record = image + lc_save_record_offset(slot, 1);
        if (get32(record + 4) == 0x4C433035U && get32(record + 8) == 0x4F473321U) return -1;
        record = image + lc_save_record_offset(slot, 0);
        if (get32(record + 4) == 0x4C433035U && get32(record + 8) == 0x4F473321U) return -1;
    }
    return 0;
}
int lc_save_expand_image(u8 *image, u32 capacity)
{
    int version = lc_save_image_version(image, capacity);
    u32 n, slot, oldOffset, offset, oldSize, size, i;
    u8 *record, *marker;
    if (version < 0) return 0;
    if (version == 1) return 1;
    /* Move later records first so no unread legacy payload is overwritten. */
    for (n = 3; n; --n) {
        slot = n == 3 ? 15 : n - 1;
        oldOffset = lc_save_record_offset(slot, 0); offset = lc_save_record_offset(slot, 1);
        oldSize = lc_save_record_size(slot, 0); size = lc_save_record_size(slot, 1);
        for (i = oldSize; i; --i) image[offset + i - 1] = image[oldOffset + i - 1];
        record = image + offset;
        if (!lc_save_expand_record(record, size, slot)) {
            /* Retain a corrupt or empty record verbatim in its corresponding
             * slot, inserting only the new allocation. Never reseal it as valid.
             */
            for (i = oldSize; i > LC_SAVE_NORMAL_BYTES; --i)
                record[i - 1 + LC_SAVE_DISCOVERY_BYTES] = record[i - 1];
            for (i = 0; i < LC_SAVE_DISCOVERY_BYTES; ++i) record[LC_SAVE_NORMAL_BYTES + i] = 0;
        }
    }
    marker = image + LC_SAVE_IMAGE_MARKER;
    put32(marker, 0x4C433035U); put32(marker + 4, 0x53524131U);
    put32(marker + 8, 0x00010020U); put32(marker + 12, marker_crc(marker));
    return 1;
}
int lc_save_pak_decode(u8 *record, u32 capacity, u32 slot)
{
    u32 expanded, length;
    if (!record || slot >= 2 || capacity < LC_SAVE_EXPANDED_NORMAL_BYTES) return 0;
    if (get32(record + 4) == 0x4C433035U && get32(record + 8) == 0x4F473321U) expanded = 1;
    else if (get32(record + 4) == 0x51756573U && get32(record + 8) == 0x744F4733U) expanded = 0;
    else return 0;
    length = lc_save_record_size(slot, expanded);
    if (get32(record) != checksums(record, length, 0)) return 0;
    lc_save_record_seal(record, capacity, slot, expanded);
    return expanded || lc_save_expand_record(record, capacity, slot);
}
int lc_save_pak_encode(u8 *record, u32 capacity, u32 slot)
{
    if (slot >= 2 || !lc_save_record_valid(record, capacity, slot, 1)) return 0;
    put32(record, checksums(record, LC_SAVE_EXPANDED_NORMAL_BYTES, 0));
    return 1;
}
