typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct { u8 unknown0[2]; u8 count; u8 flags; u16 width; u16 height; } Header;
extern u32 func_00201E38(s32, u32);
extern void *func_00203414(Header *, u32);

void *func_00203570(Header *header, u32 index)
{
    u32 width = header->width;
    u32 height;
    u8 *record;
    s32 offset;
    s32 format;

    width = (width << 16) >> 16;
    height = header->height;
    record = func_00203414(header, index);
    if (record == 0) return 0;
    offset = 8;
    if (record[3] & 1) {
        if (header->flags & 2) {
            format = 3;
        } else {
            format = 2;
            if (header->flags & 4) format = 1;
        }
        offset = (s32)func_00201E38(format, width) * (u16)height + 8;
    }
    if (record[3] & 2) {
        if (header->flags & 1) {
            offset += (s32)func_00201E38(0, width) * (u16)height;
        } else {
            offset += (s32)func_00201E38(1, width) * (u16)height;
        }
    }
    if (record[3] & 4) {
        height = (u32)(record + offset);
        return (void *)height;
    }
    return 0;
}
