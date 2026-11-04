#include "save_compression.h"
#include "save_format.h"
typedef unsigned char ImageByte;
typedef unsigned int ImageWord;

static void image_put(ImageByte *p, ImageWord value)
{
    p[0] = (ImageByte)(value >> 24); p[1] = (ImageByte)(value >> 16);
    p[2] = (ImageByte)(value >> 8); p[3] = (ImageByte)value;
}
int lc_save_commit_compressed(ImageByte *image, ImageWord capacity,
                             const ImageByte *source, ImageWord size,
                             ImageByte *scratch, ImageWord scratch_size)
{
    ImageWord packed, i, format, offset, available;
    ImageByte *decoded, *marker;
    unsigned short *work;
    int version;
    if (!image || !source || !scratch || capacity < LC_SAVE_PACK_IMAGE_BYTES ||
        (size != LC_SAVE_EXPANDED_SUSPEND_BYTES && size != LC_SAVE_LARGE_SUSPEND_BYTES && size != LC_SAVE_CAPACITY_SUSPEND_BYTES &&
         size != LC_SAVE_CUSTOM_SUSPEND_BYTES && size != LC_SAVE_CUSTOM_LARGE_SUSPEND_BYTES && size != LC_SAVE_CUSTOM_CAPACITY_SUSPEND_BYTES) ||
        scratch_size < LC_SAVE_PACK_COMMIT_WORK_BYTES(size)) return 0;
    version = lc_save_image_version(image, capacity);
    if (version < 1 || version > 4) return 0;
    format = size == LC_SAVE_CUSTOM_CAPACITY_SUSPEND_BYTES ? 5 : size == LC_SAVE_CUSTOM_LARGE_SUSPEND_BYTES ? 6 : size == LC_SAVE_CUSTOM_SUSPEND_BYTES ? 4 : size == LC_SAVE_CAPACITY_SUSPEND_BYTES ? 3 : size == LC_SAVE_LARGE_SUSPEND_BYTES ? 2 : 1;
    if ((format >= 4) != (version >= 3)) return 0;
    offset = format >= 4 ? LC_SAVE_PACK_CUSTOM_SUSPEND_OFFSET : LC_SAVE_PACK_SUSPEND_OFFSET;
    available = LC_SAVE_PACK_IMAGE_MARKER - offset;
    if (!lc_save_record_valid(source, size, 15, format)) return 0;
    decoded = scratch + LC_SAVE_PACK_SUSPEND_CAPACITY;
    /* The encoder has finished using its hash chains before readback starts.
     * Reuse those bytes for the decoded verification copy. The work region
     * is larger than size for every supported record. */
    work = (unsigned short *)decoded;
    packed = lc_save_frame_encode(source, size, scratch, available, work);
    if (!packed || lc_save_frame_decode(decoded, size, scratch, packed) != size) return 0;
    for (i = 0; i < size; ++i) if (source[i] != decoded[i]) return 0;
    /* Only this point publishes data. Keep options, both regular saves and all
     * bytes outside the new frame/format marker, including old unused bytes.
     */
    for (i = 0; i < packed; ++i) image[offset + i] = scratch[i];
    marker = image + LC_SAVE_PACK_IMAGE_MARKER;
    image_put(marker, format >= 4 ? 0x4C433038U : 0x4C433036U); image_put(marker + 4, format >= 4 ? 0x53524134U : 0x53524132U);
    image_put(marker + 8, format >= 4 ? 0x00040060U : 0x00020020U); image_put(marker + 12, lc_save_data_crc(marker, 12));
    return 1;
}
ImageWord lc_save_read_compressed(const ImageByte *image, ImageWord capacity,
                                 ImageByte *output, ImageWord output_capacity)
{
    ImageWord size, format, offset;
    int version;
    if (!image || !output || capacity < LC_SAVE_PACK_IMAGE_BYTES) return 0;
    version = lc_save_image_version(image, capacity);
    if (version != 2 && version != 4) return 0;
    offset = version == 4 ? LC_SAVE_PACK_CUSTOM_SUSPEND_OFFSET : LC_SAVE_PACK_SUSPEND_OFFSET;
    size = lc_save_frame_decode(output, output_capacity, image + offset, LC_SAVE_PACK_IMAGE_MARKER - offset);
    if (version == 4) {
        if (size != LC_SAVE_CUSTOM_SUSPEND_BYTES && size != LC_SAVE_CUSTOM_LARGE_SUSPEND_BYTES && size != LC_SAVE_CUSTOM_CAPACITY_SUSPEND_BYTES) return 0;
        format = size == LC_SAVE_CUSTOM_CAPACITY_SUSPEND_BYTES ? 5 : size == LC_SAVE_CUSTOM_LARGE_SUSPEND_BYTES ? 6 : 4;
    } else {
        if (size != LC_SAVE_EXPANDED_SUSPEND_BYTES && size != LC_SAVE_LARGE_SUSPEND_BYTES && size != LC_SAVE_CAPACITY_SUSPEND_BYTES) return 0;
        format = size == LC_SAVE_CAPACITY_SUSPEND_BYTES ? 3 : size == LC_SAVE_LARGE_SUSPEND_BYTES ? 2 : 1;
    }
    return lc_save_record_valid(output, size, 15, format) ? size : 0;
}
int lc_save_upgrade_image(ImageByte *image, ImageWord capacity, ImageByte *scratch, ImageWord scratch_size)
{
    int version = lc_save_image_version(image, capacity);
    ImageWord i, n, slot, old_offset, offset, old_size, size = 0, format = 1;
    ImageByte *staged, *logical, *work, *record, *marker;
    if (version < 0) return 0;
    if (version >= 3) return 1;
    if (!scratch || scratch_size < LC_SAVE_UPGRADE_WORK_BYTES) return 0;
    staged = scratch; logical = staged + LC_SAVE_IMAGE_BYTES;
    work = logical + LC_SAVE_CUSTOM_CAPACITY_SUSPEND_BYTES;
    for (i = 0; i < LC_SAVE_IMAGE_BYTES; ++i) staged[i] = image[i];
    if (version == 2) {
        size = lc_save_read_compressed(image, capacity, logical, LC_SAVE_CUSTOM_CAPACITY_SUSPEND_BYTES);
        if (!size) return 0;
        format = size == LC_SAVE_CAPACITY_SUSPEND_BYTES ? 3 : size == LC_SAVE_LARGE_SUSPEND_BYTES ? 2 : 1;
        if (!lc_save_upgrade_record(logical, LC_SAVE_CUSTOM_CAPACITY_SUSPEND_BYTES, 15, format)) return 0;
        size += LC_SAVE_CUSTOM_FLAG_BYTES;
    } else if (!lc_save_expand_image(staged, LC_SAVE_IMAGE_BYTES)) return 0;
    /* Later slots first; no byte from an unread record is overwritten. Invalid
     * slots keep their original payload and signature and remain invalid. */
    for (n = version == 2 ? 2 : 3; n; --n) {
        slot = n == 3 ? 15 : n - 1;
        old_offset = lc_save_record_offset(slot, 1); offset = lc_save_record_offset(slot, 4);
        old_size = lc_save_record_size(slot, 1);
        for (i = old_size; i; --i) staged[offset + i - 1] = staged[old_offset + i - 1];
        record = staged + offset;
        if (!lc_save_upgrade_record(record, lc_save_record_size(slot, 4), slot, 1)) {
            for (i = old_size; i > LC_SAVE_CUSTOM_FLAG_OFFSET; --i)
                record[i - 1 + LC_SAVE_CUSTOM_FLAG_BYTES] = record[i - 1];
            for (i = 0; i < LC_SAVE_CUSTOM_FLAG_BYTES; ++i) record[LC_SAVE_CUSTOM_FLAG_OFFSET + i] = 0;
        }
    }
    marker = staged + LC_SAVE_CUSTOM_IMAGE_MARKER;
    image_put(marker, 0x4C433037U); image_put(marker + 4, 0x53524133U);
    image_put(marker + 8, 0x00030060U); image_put(marker + 12, lc_save_data_crc(marker, 12));
    /* Remove the superseded compressed marker from its authenticated location;
     * raw conversion otherwise preserves bytes outside the owned allocation. */
    if (version == 2) {
        for (i = 0; i < 16; ++i) staged[LC_SAVE_PACK_IMAGE_MARKER + i] = 0;
        if (!lc_save_commit_compressed(staged, LC_SAVE_IMAGE_BYTES, logical, size, work,
                                      LC_SAVE_PACK_COMMIT_WORK_BYTES(LC_SAVE_CUSTOM_CAPACITY_SUSPEND_BYTES))) return 0;
    }
    for (i = 0; i < LC_SAVE_IMAGE_BYTES; ++i) image[i] = staged[i];
    return 1;
}
