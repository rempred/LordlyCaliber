#include "sprite_readers.h"
#include "stride.h"
static unsigned half(const uint8_t *p) { return (unsigned)p[0] * 256 + p[1]; }

/* func_00203414's flag/stride calculation and child walk, without N64 pointer
 * casts or native-endian struct aliases. Decode all child offsets in one pass.
 * Header/buffer validation belongs to this host adaptation, not the original. */
int lordly_sprite_layout(const uint8_t *source, size_t size, LordlySpriteLayout *layout) {
    size_t at = 8, primary, secondary;
    unsigned flags, i;
    if (!source || !layout || size < 8 || size > 64 * 1024 * 1024 || half(source) != 0x5554)
        return 1;
    layout->width = half(source + 4);
    layout->height = half(source + 6);
    layout->count = source[2];
    if (!layout->count || !layout->width || !layout->height || layout->width > 8192 ||
        layout->height > 8192 || layout->width * layout->height > 4194304) return 1;
    flags = source[3];
    layout->color_format = flags & 2 ? 3 : flags & 4 ? 1 : 2;
    layout->alpha_format = flags & 1 ? 0 : 1;
    layout->color_stride = func_00201E38((int)layout->color_format, layout->width);
    /* The accepted reader explicitly gives RGBA32 no secondary bytes. */
    layout->alpha_stride = flags & 2 ? 0 : func_00201E38((int)layout->alpha_format, layout->width);
    primary = (size_t)layout->color_stride * layout->height;
    secondary = (size_t)layout->alpha_stride * layout->height;
    for (i = 0; i < layout->count; ++i) {
        LordlySpriteChild *child = layout->children + i;
        if (at > size || size - at < 8 || half(source + at) != 0x5554) return 1;
        child->discriminator = source[at + 2];
        child->flags = source[at + 3];
        child->width = (uint16_t)half(source + at + 4);
        child->height = (uint16_t)half(source + at + 6);
        if (child->discriminator >= layout->count) return 1;
        child->color = child->alpha = child->palette = SIZE_MAX;
        at += 8;
        if (child->flags & 1) { child->color = at; at += primary; }
        if ((child->flags & 2) && secondary) { child->alpha = at; at += secondary; }
        if (child->flags & 4) { child->palette = at; at += 512; }
        if (at > size) return 1;
    }
    return at != size;
}

/* Portable kernels from func_00203664: nibble/byte/halfword subtraction and
 * RGBA32 XOR. Resource allocation, recursive child resolution and cycle checks
 * stay with SpriteObject. Byte-wise XOR is independent of host byte order. */
int lordly_sprite_delta(uint8_t *destination, const uint8_t *previous,
    const uint8_t *delta, size_t size, unsigned format) {
    size_t i;
    if (!destination || !previous || !delta || !size || format > 3 ||
        (format == 2 && size % 2) || (format == 3 && size % 4)) return 1;
    for (i = 0; i < size;) {
        if (format == 0) {
            unsigned high = (previous[i] & 0xf0) - (delta[i] & 0xf0);
            unsigned low = (previous[i] & 0x0f) - (delta[i] & 0x0f);
            destination[i] = (uint8_t)((high & ~0x0fU) | (low & 0x0f));
            ++i;
        } else if (format == 1) {
            destination[i] = (uint8_t)(previous[i] - delta[i]);
            ++i;
        } else if (format == 2) {
            unsigned value = half(previous + i) - half(delta + i);
            destination[i] = (uint8_t)(value >> 8);
            destination[i + 1] = (uint8_t)value;
            i += 2;
        } else {
            destination[i] = previous[i] ^ delta[i];
            ++i;
        }
    }
    return 0;
}
