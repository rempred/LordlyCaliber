typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef struct { u8 unknown0[3]; u8 format; u16 width; u16 height; } Header;
extern u32 func_00202198(u32);
extern u16 func_00202258(u16);

static __inline__ u32 row_stride(u32 width, u32 format)
{
    if (format == 3) return ((width + 3) >> 2) * 16;
    if (format == 2) return ((width + 3) >> 2) * 8;
    if (format == 1) return ((width + 7) >> 3) * 8;
    if (format == 0) return ((width + 15) >> 4) * 8;
    return ((width + 31) >> 5) * 8;
}

void func_002032B8(Header *header)
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
                ((u32 *)row32)[x] = func_00202198(((u32 *)row32)[x]);
            }
        }
    } else if (header->format == 2) {
        row16 = (u8 *)(header + 1);
        for (y = 0; y < height; y++, row16 += stride) {
            for (x = 0; x < width; x++) {
                ((u16 *)row16)[x] = func_00202258(((u16 *)row16)[x]);
            }
        }
    }
}
