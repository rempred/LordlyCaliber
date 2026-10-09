typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef struct { u8 unknown0[3]; u8 format; u16 width; u16 height; } Header;
typedef struct { u8 red, green, blue, alpha; } Color;

static __inline__ u32 row_stride(u32 width, u32 format)
{
    if (format == 3) return ((width + 3) >> 2) * 16;
    if (format == 2) return ((width + 3) >> 2) * 8;
    if (format == 1) return ((width + 7) >> 3) * 8;
    if (format == 0) return ((width + 15) >> 4) * 8;
    return ((width + 31) >> 5) * 8;
}

static __inline__ u32 convert32(u32 pixel)
{
    Color color = *(Color *)&pixel;
    u32 weighted = color.red * 30 + color.green * 59 + color.blue * 11;
    color.red = weighted * 255 / 30000;
    color.green = weighted / 150;
    color.blue = weighted * 130 / 30000;
    return *(u32 *)&color;
}

static __inline__ u16 convert16(u16 pixel)
{
    u32 color = pixel;
    u32 red = (color >> 8) & 0xF8;
    u32 green = (color >> 3) & 0xF8;
    u32 blue = (color << 2) & 0xF8;
    u32 weighted = red * 30 + green * 59 + blue * 11;
    red = weighted * 255 / 30000;
    green = weighted / 150;
    blue = weighted * 130 / 30000;
    pixel = ((red & 0xF8) << 8) + ((green & 0xF8) << 3)
        + ((blue & 0xF8) >> 2) | (pixel & 1);
    return pixel;
}

void func_00203030(Header *header)
{
    u8 *row32;
    u8 *row16;
    u32 stride;
    u32 x;
    u32 y;
    u32 width = header->width;
    u32 height = header->height;

    stride = row_stride(width, header->format);
    if (header->format == 3) {
        row32 = (u8 *)(header + 1);
        for (y = 0; y < height; y++, row32 += stride) {
            for (x = 0; x < width; x++) {
                ((u32 *)row32)[x] = convert32(((u32 *)row32)[x]);
            }
        }
    } else if (header->format == 2) {
        row16 = (u8 *)(header + 1);
        for (y = 0; y < height; y++, row16 += stride) {
            for (x = 0; x < width; x++) {
                ((u16 *)row16)[x] = convert16(((u16 *)row16)[x]);
            }
        }
    }
}
