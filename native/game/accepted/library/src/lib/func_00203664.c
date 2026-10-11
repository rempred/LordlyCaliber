typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct { u8 magic0, magic1, kind, flags; u16 width, height; } Header;
extern Header *func_00203414(Header *, u32);
extern void *func_00203570(Header *, u32);
extern u32 func_00201E38(s32, u32);
extern void *resource_alloc_tree_scan(u32);
extern void resource_free(void *);
extern void memcpy(void *source, void *destination, u32 size);

static __inline__ u8 *primary_data(Header *header, u32 index)
{
    Header *record = func_00203414(header, index);
    if (record == 0) return 0;
    if (record->flags & 1) return (u8 *)(record + 1);
    return 0;
}

static __inline__ u8 *secondary_data(Header *header, u32 index)
{
    Header *record = func_00203414(header, index);
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

static __inline__ u32 subtract_nibbles(u8 previous, u8 delta, u32 highMask)
{
    u32 high = (previous & 0xF0) - (delta & 0xF0);
    u32 low;
    high &= highMask;
    low = (previous & 0x0F) - (delta & 0x0F);
    return (low & 0x0F) | high;
}

static __inline__ u32 subtract_prefix_nibbles(u8 previous, u8 delta)
{
    u32 high;
    u32 low;
    do {
        high = previous & 0xF0;
    } while (0);
    high -= delta & 0xF0;
    low = (previous & 0x0F) - (delta & 0x0F);
    return (low & 0x0F) | high;
}

static __inline__ s32 byte_count(s32 format, u32 width, u32 height)
{
    return func_00201E38(format, width) * height;
}

Header *func_00203664(Header *header, u32 index)
{
    u32 width = header->width;
    u32 height = header->height;
    s32 primaryStride, alphaStride;
    s32 primaryBytes, alphaBytes;
    Header *record;
    Header *result;
    Header *previous;
    Header *resultRecord;
    if (header->flags & 2) {
        primaryStride = ((width + 3) >> 2) * 16;
        alphaStride = 0;
    } else if (header->flags & 4) {
        primaryStride = ((width + 7) >> 3) * 8;
        if (header->flags & 1) alphaStride = ((width + 15) >> 4) * 8;
        else alphaStride = primaryStride;
    } else {
        primaryStride = ((width + 3) >> 2) * 8;
        if (header->flags & 1) alphaStride = ((width + 15) >> 4) * 8;
        else alphaStride = ((width + 7) >> 3) * 8;
    }
    primaryBytes = primaryStride * (s32)height;
    alphaBytes = alphaStride * (s32)height;
    record = func_00203414(header, index);
    if (index == record->kind) {
        u32 size = 16;
        if (record->flags & 1) size += primaryBytes;
        if (record->flags & 2) size += alphaBytes;
        if (record->flags & 4) size += 512;
        result = resource_alloc_tree_scan(size);
        memcpy(header, result, 8);
        result->kind = 1;
        memcpy(record, result + 1, size - 8);
        ((Header *)(result + 1))->kind = 0;
        return result;
    } else {
        u32 size;
        previous = func_00203664(header, record->kind);
        size = 16;
        if (record->flags & 1) size += primaryBytes;
        if (record->flags & 2) size += alphaBytes;
        if (record->flags & 4) size += 512;
        result = resource_alloc_tree_scan(size);
        memcpy(header, result, 8);
        resultRecord = result + 1;
        result->kind = 1;
        memcpy(record, resultRecord, 8);
        resultRecord->kind = 0;
        if (header->flags & 2) {
            u32 *deltaData = (u32 *)primary_data(header, index);
            u32 *previousData = (u32 *)primary_data(previous, 0);
            u32 *outputData = (u32 *)primary_data(result, 0);
            s32 count = (func_00201E38(3, width) * height) >> 2;
            while (count--) {
                *outputData++ = *previousData++ ^ *deltaData++;
            }
        } else if (header->flags & 4) {
            u8 *deltaData = primary_data(header, index);
            u8 *deltaAlpha = secondary_data(header, index);
            u8 *previousData = primary_data(previous, 0);
            u8 *previousAlpha = secondary_data(previous, 0);
            u8 *outputData = primary_data(result, 0);
            u8 *outputAlpha = secondary_data(result, 0);
            if (resultRecord->flags & 1) {
                if (resultRecord->flags & 8) {
                    memcpy(deltaData, outputData, func_00201E38(1, width) * height);
                } else {
                    {
                        u32 size = func_00201E38(1, width) * height;
                        if (size & 1) {
                            {
                                u8 deltaValue = *deltaData++;
                                u8 previousValue = *previousData++;
                                size -= 1;
                                *outputData++ = previousValue - deltaValue;
                            }
                        }
                        if (size & 2) {
                            {
                                u8 deltaValue = *deltaData++;
                                u8 previousValue = *previousData++;
                                size -= 2;
                                *outputData++ = previousValue - deltaValue;
                            }
                            {
                                u8 previousValue = *previousData;
                                u8 deltaValue = *deltaData++;
                                previousData++;
                                *outputData++ = previousValue - deltaValue;
                            }
                        }
                        while (size) {
                            {
                                u8 deltaValue = *deltaData++;
                                u8 previousValue = *previousData++;
                                size -= 4;
                                *outputData++ = previousValue - deltaValue;
                            }
                            {
                                u8 previousValue = *previousData;
                                u8 deltaValue = *deltaData++;
                                previousData++;
                                *outputData++ = previousValue - deltaValue;
                            }
                            {
                                u8 previousValue = *previousData;
                                u8 deltaValue = *deltaData++;
                                previousData++;
                                *outputData++ = previousValue - deltaValue;
                            }
                            {
                                u8 previousValue = *previousData;
                                u8 deltaValue = *deltaData++;
                                previousData++;
                                *outputData++ = previousValue - deltaValue;
                            }
                        }
                    }
                }
            }
            if (resultRecord->flags & 2) {
                if (header->flags & 1) {
                    {
                        u32 size = func_00201E38(0, width) * height;
                        if (size & 1) {
                            {
                                u8 deltaValue = *deltaAlpha++;
                                u8 previousValue = *previousAlpha++;
                                size -= 1;
                                *outputAlpha++ = subtract_nibbles(previousValue, deltaValue, ~0U);
                            }
                        }
                        if (size & 2) {
                            u32 highMask;
                            do {
                                highMask = ~0x0F;
                            } while (0);
                            {
                                u8 deltaValue = *deltaAlpha++;
                                u8 previousValue = *previousAlpha++;
                                size -= 2;
                                *outputAlpha++ = subtract_prefix_nibbles(previousValue, deltaValue);
                            }
                            {
                                u8 deltaValue = *deltaAlpha++;
                                u8 previousValue = *previousAlpha++;
                                *outputAlpha++ = subtract_nibbles(previousValue, deltaValue, highMask);
                            }
                        }
                        while (size) {
                            u32 highMask = ~0x0F;
                            {
                                u8 deltaValue = *deltaAlpha++;
                                u8 previousValue = *previousAlpha++;
                                size -= 4;
                                *outputAlpha++ = subtract_nibbles(previousValue, deltaValue, highMask);
                            }
                            {
                                u8 previousValue = *previousAlpha;
                                u8 deltaValue = *deltaAlpha++;
                                previousAlpha++;
                                *outputAlpha++ = subtract_nibbles(previousValue, deltaValue, highMask);
                            }
                            {
                                u8 previousValue = *previousAlpha;
                                u8 deltaValue = *deltaAlpha++;
                                previousAlpha++;
                                *outputAlpha++ = subtract_nibbles(previousValue, deltaValue, highMask);
                            }
                            {
                                u8 previousValue = *previousAlpha;
                                u8 deltaValue = *deltaAlpha++;
                                previousAlpha++;
                                *outputAlpha++ = subtract_nibbles(previousValue, deltaValue, highMask);
                            }
                        }
                    }
                } else {
                    {
                        u32 size = func_00201E38(1, width) * height;
                        if (size & 1) {
                            {
                                u8 deltaValue = *deltaAlpha++;
                                u8 previousValue = *previousAlpha++;
                                size -= 1;
                                *outputAlpha++ = previousValue - deltaValue;
                            }
                        }
                        if (size & 2) {
                            {
                                u8 deltaValue = *deltaAlpha++;
                                u8 previousValue = *previousAlpha++;
                                size -= 2;
                                *outputAlpha++ = previousValue - deltaValue;
                            }
                            {
                                u8 previousValue = *previousAlpha;
                                u8 deltaValue = *deltaAlpha++;
                                previousAlpha++;
                                *outputAlpha++ = previousValue - deltaValue;
                            }
                        }
                        while (size) {
                            do {
                                {
                                    u8 deltaValue = *deltaAlpha++;
                                    u8 previousValue = *previousAlpha++;
                                    size -= 4;
                                    *outputAlpha++ = previousValue - deltaValue;
                                }
                                {
                                    u8 previousValue = *previousAlpha;
                                    u8 deltaValue = *deltaAlpha++;
                                    previousAlpha++;
                                    *outputAlpha++ = previousValue - deltaValue;
                                }
                            } while (0);
                            {
                                u8 previousValue = *previousAlpha;
                                u8 deltaValue = *deltaAlpha++;
                                previousAlpha++;
                                *outputAlpha++ = previousValue - deltaValue;
                            }
                            {
                                u8 previousValue = *previousAlpha;
                                u8 deltaValue = *deltaAlpha++;
                                previousAlpha++;
                                *outputAlpha++ = previousValue - deltaValue;
                            }
                        }
                    }
                }
            }
            if (resultRecord->flags & 4) {
                void *palette = func_00203570(header, index);
                void *outputPalette = func_00203570(result, 0);
                memcpy(palette, outputPalette, 512);
            }
        } else if (!(header->flags & 6)) {
            u16 *deltaData = (u16 *)primary_data(header, index);
            u8 *deltaAlpha = secondary_data(header, index);
            u16 *previousData = (u16 *)primary_data(previous, 0);
            u8 *previousAlpha = secondary_data(previous, 0);
            u16 *outputData = (u16 *)primary_data(result, 0);
            u8 *outputAlpha = secondary_data(result, 0);
            if (resultRecord->flags & 1) {
                if (resultRecord->flags & 8) {
                    do {
                        memcpy(deltaData, outputData, func_00201E38(2, width) * height);
                    } while (0);
                } else {
                    s32 count = (func_00201E38(2, width) * height) >> 1;
                    while (count--) {
                        *outputData++ = *previousData++ - *deltaData++;
                    }
                }
            }
            if (resultRecord->flags & 2) {
                if (header->flags & 1) {
                    s32 count = byte_count(0, width, height);
                    while (count--) {
                        u8 previousValue = *previousAlpha++;
                        u8 deltaValue = *deltaAlpha++;
                        *outputAlpha++ = subtract_nibbles(previousValue, deltaValue, ~0x0F);
                    }
                } else {
                    s32 count = byte_count(1, width, height);
                    while (count--) {
                        *outputAlpha++ = *previousAlpha++ - *deltaAlpha++;
                    }
                }
            }
        }
        resource_free(previous);
        return result;
    }
}
