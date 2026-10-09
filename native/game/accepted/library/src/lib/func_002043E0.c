typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct { u8 magic0, magic1, kind, flags; u16 width, height; } Header;
extern Header *func_00203664(Header *, u32);
extern void *func_00203414(Header *, u32);
extern u32 func_00201E38(s32, u32);
extern void *resource_alloc(u32);
extern void resource_free(void *);
extern void memcpy(void *source, void *destination, u32 size);
extern void func_00203030(Header *);
extern void func_002032B8(Header *);

static __inline__ u8 *primary_data(Header *header)
{
    Header *record = func_00203414(header, 0);
    if (record == 0) return 0;
    if (record->flags & 1) return (u8 *)(record + 1);
    return 0;
}

static __inline__ u8 *secondary_data(Header *header)
{
    Header *record = func_00203414(header, 0);
    u32 width;
    s32 height;
    u8 flags;
    s32 format;
    if (record == 0) return 0;
    flags = record->flags;
    width = header->width;
    do {
        height = header->height;
    } while (0);
    if (flags & 2) {
        if (flags & 1) {
            if (header->flags & 2) {
                format = 3;
            } else {
                format = 2;
                if (header->flags & 4) format = 1;
            }
            do {
                return (u8 *)record + ((s32)func_00201E38(format, width) * height + 8);
            } while (0);
        }
        return (u8 *)(record + 1);
    }
    return 0;
}


static __inline__ u32 expand_pixel(u32 pixel)
{
    u32 result = ((u32)(pixel & 0xF800) << 16)
               | ((u32)(pixel & 0x07C0) << 13);
    result |= (pixel & 0x003E) << 10;
    if (pixel & 1) result |= 0xFF;
    return result;
}
static __inline__ u32 expand_alpha(u32 pixel, u32 alpha)
{
    u32 result = ((u32)(pixel & 0xF800) << 16)
               | ((u32)(pixel & 0x07C0) << 13)
               | ((u32)(pixel & 0x003E) << 10);
    result |= alpha;
    return result;
}
Header *func_002043E0(Header *input, u32 argument1, u16 *table, u8 *valueOut, u32 options)
{
    Header *source = func_00203664(input, argument1);
    s32 sourceStride;
    u32 width, height;
    s32 destinationStride, alphaStride;
    Header *result;
    s32 primaryBytes;
    u32 *destination;
    u32 x, y;
    if (source == 0) return 0;
    if (valueOut != 0) *valueOut = ((u8 *)source)[12];
    width = source->width;
    height = source->height;
    destinationStride = ((width + 3) >> 2) * 16;
    primaryBytes = destinationStride * (s32)height;
    if (source->flags & 2) {
        sourceStride = destinationStride;
        alphaStride = 0;
    } else if (source->flags & 4) {
        sourceStride = ((width + 7) >> 3) * 8;
        if (source->flags & 1) alphaStride = ((width + 15) >> 4) * 8;
        else alphaStride = sourceStride;
    } else {
        sourceStride = ((width + 3) >> 2) * 8;
        if (source->flags & 1) alphaStride = ((width + 15) >> 4) * 8;
        else alphaStride = ((width + 7) >> 3) * 8;
    }
    result = resource_alloc(primaryBytes + 8);
    result->magic0 = 0x36;
    result->magic1 = 0x34;
    result->kind = 0;
    result->flags = 3;
    result->width = width;
    result->height = height;
    destination = (u32 *)(result + 1);
    if (!(source->flags & 2)) {
        if (source->flags & 4) {
            u8 *primary;
            u8 *alpha;
            primary = primary_data(source);
            alpha = secondary_data(source);
            if (source->flags & 1) {
                for (y = 0; y < height; y++) {
                    for (x = 0; x < width; x += 2) {
                        u32 pixel = expand_pixel(table[primary[x]]);
                        u32 highAlpha;
                        u32 lowAlpha;
                        u32 secondPixel;
                        pixel &= 0xFFFFFF00;
                        highAlpha = alpha[x >> 1] & 0xF0;
                        pixel |= highAlpha;
                        pixel |= highAlpha >> 4;
                        destination[x] = pixel;
                        secondPixel = expand_pixel(table[primary[x + 1]]);
                        secondPixel &= 0xFFFFFF00;
                        lowAlpha = alpha[x >> 1] & 0x0F;
                        destination[x + 1] = secondPixel | (lowAlpha << 4) | lowAlpha;
                    }
                    destination = (u32 *)((u8 *)destination + destinationStride);
                    primary += sourceStride;
                    alpha += alphaStride;
                }
            } else {
                for (y = 0; y < height; y++) {
                    for (x = 0; x < width; x++) {
                        u32 pixel = expand_pixel(table[primary[x]]);
                        destination[x] = (pixel & 0xFFFFFF00) | alpha[x];
                    }
                    destination = (u32 *)((u8 *)destination + destinationStride);
                    primary += sourceStride;
                    alpha += alphaStride;
                }
            }
        } else {
            u16 *primary;
            u8 *alpha;
            primary = (u16 *)primary_data(source);
            alpha = secondary_data(source);
            if (source->flags & 1) {
                for (y = 0; y < height; y++) {
                    for (x = 0; x < width; x += 2) {
                        u32 pixel = expand_pixel(primary[x]);
                        u32 highAlpha;
                        u32 lowAlpha;
                        u32 secondPixel;
                        pixel &= 0xFFFFFF00;
                        highAlpha = alpha[x >> 1] & 0xF0;
                        pixel |= highAlpha;
                        pixel |= highAlpha >> 4;
                        destination[x] = pixel;
                        secondPixel = expand_pixel(primary[x + 1]);
                        secondPixel &= 0xFFFFFF00;
                        lowAlpha = alpha[x >> 1] & 0x0F;
                        destination[x + 1] = secondPixel | (lowAlpha << 4) | lowAlpha;
                    }
                    destination = (u32 *)((u8 *)destination + destinationStride);
                    primary = (u16 *)((u8 *)primary + sourceStride);
                    alpha += alphaStride;
                }
            } else {
                for (y = 0; y < height; y++) {
                    u16 *sourceRow = (u16 *)primary;
                    u8 *alphaRow = alpha;
                    for (x = 0; x < width; x += 2) {
                        destination[x] = expand_alpha(*sourceRow++, *alphaRow++);
                        destination[x + 1] = expand_alpha(*sourceRow++, *alphaRow++);
                    }
                    destination = (u32 *)((u8 *)destination + destinationStride);
                    primary = (u16 *)((u8 *)primary + sourceStride);
                    alpha += alphaStride;
                }
            }
        }
    }
    resource_free(source);
    if (options & 1) func_00203030(result);
    if (options & 4) func_002032B8(result);
    return result;
}
