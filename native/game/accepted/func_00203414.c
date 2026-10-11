typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct { u8 unknown0[2]; u8 count; u8 flags; u16 width; u16 height; } Header;
extern u32 func_00201E38(s32, u32);

void *func_00203414(Header *header, u32 index)
{
    u32 width;
    u32 height;
    u32 primaryBytes;
    u32 secondaryBytes;
    u32 offset;
    u32 i;
    u8 *record;
    u8 count;
    u8 flags;
    u8 recordFlags;

    if (header == 0) return 0;
    flags = header->flags;
    width = header->width;
    height = header->height;
    width <<= 16;
    width >>= 16;
    height <<= 16;
    height >>= 16;
    if (flags & 2) {
        primaryBytes = func_00201E38(3, width) * height;
        secondaryBytes = 0;
    } else if (flags & 4) {
        primaryBytes = func_00201E38(1, width) * height;
        if (header->flags & 1) {
            secondaryBytes = func_00201E38(0, width) * height;
        } else {
            secondaryBytes = func_00201E38(1, width) * height;
        }
    } else {
        primaryBytes = func_00201E38(2, width) * height;
        if (header->flags & 1) {
            secondaryBytes = func_00201E38(0, width) * height;
        } else {
            secondaryBytes = func_00201E38(1, width) * height;
        }
    }

    offset = 8;
    count = header->count;
    record = (u8 *)header + offset;
    for (i = 0; i < count; i++) {
        if (i == index) return record;
        recordFlags = record[3];
        offset += 8;
        if (recordFlags & 1) offset += primaryBytes;
        if (recordFlags & 2) offset += secondaryBytes;
        if (recordFlags & 4) offset += 0x200;
        record = (u8 *)header + offset;
    }
    return 0;
}
