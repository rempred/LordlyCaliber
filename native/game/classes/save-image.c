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
    ImageWord packed, i, format;
    ImageByte *decoded, *marker;
    unsigned short *work;
    int version;
    if (!image || !source || !scratch || capacity < LC_SAVE_PACK_IMAGE_BYTES ||
        (size != LC_SAVE_EXPANDED_SUSPEND_BYTES && size != LC_SAVE_LARGE_SUSPEND_BYTES) ||
        scratch_size < LC_SAVE_PACK_COMMIT_WORK_BYTES(size)) return 0;
    version = lc_save_image_version(image, capacity);
    if (version != 1 && version != 2) return 0;
    format = size == LC_SAVE_LARGE_SUSPEND_BYTES ? 2 : 1;
    if (!lc_save_record_valid(source, size, 15, format)) return 0;
    decoded = scratch + LC_SAVE_PACK_SUSPEND_CAPACITY;
    work = (unsigned short *)(decoded + ((size + 1U) & ~1U));
    packed = lc_save_frame_encode(source, size, scratch, LC_SAVE_PACK_SUSPEND_CAPACITY, work);
    if (!packed || lc_save_frame_decode(decoded, size, scratch, packed) != size) return 0;
    for (i = 0; i < size; ++i) if (source[i] != decoded[i]) return 0;
    /* Only this point publishes data. Keep options, both regular saves and all
     * bytes outside the new frame/format marker, including old unused bytes.
     */
    for (i = 0; i < packed; ++i) image[LC_SAVE_PACK_SUSPEND_OFFSET + i] = scratch[i];
    marker = image + LC_SAVE_PACK_IMAGE_MARKER;
    image_put(marker, 0x4C433036U); image_put(marker + 4, 0x53524132U);
    image_put(marker + 8, 0x00020020U); image_put(marker + 12, lc_save_data_crc(marker, 12));
    return 1;
}
ImageWord lc_save_read_compressed(const ImageByte *image, ImageWord capacity,
                                 ImageByte *output, ImageWord output_capacity)
{
    ImageWord size, format;
    if (!image || !output || capacity < LC_SAVE_PACK_IMAGE_BYTES || lc_save_image_version(image, capacity) != 2) return 0;
    size = lc_save_frame_decode(output, output_capacity, image + LC_SAVE_PACK_SUSPEND_OFFSET, LC_SAVE_PACK_SUSPEND_CAPACITY);
    if (size != LC_SAVE_EXPANDED_SUSPEND_BYTES && size != LC_SAVE_LARGE_SUSPEND_BYTES) return 0;
    format = size == LC_SAVE_LARGE_SUSPEND_BYTES ? 2 : 1;
    return lc_save_record_valid(output, size, 15, format) ? size : 0;
}
