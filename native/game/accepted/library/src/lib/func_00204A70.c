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
    height = header->height;
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

/* The direct path needs distinct KMC temporary lifetimes. */
static __inline__ u8 *secondary_data_direct(Header *header)
{
    Header *record;
    u32 width;
    s32 height;
    u8 flags;
    s32 format;
    do {
        record = func_00203414(header, 0);
    } while (0);
    if (record == 0) return 0;
    flags = record->flags;
    width = header->width;
    height = header->height;
    if (flags & 2) {
        if (flags & 1) {
            if (header->flags & 2) {
                format = 3;
            } else {
                format = 2;
                if (header->flags & 4) format = 1;
            }
            do {
                do {
                    do {
                        do {
                            do {
                                do {
                                    do {
                                        return (u8 *)record + ((s32)func_00201E38(format, width) * height + 8);
                                    } while (0);
                                } while (0);
                            } while (0);
                        } while (0);
                    } while (0);
                } while (0);
            } while (0);
        }
        return (u8 *)(record + 1);
    }
    return 0;
}

/* Single-iteration blocks preserve the pinned compiler's allocation.
 * Flattening them requires a new linked-byte comparison. */
Header *func_00204A70(Header *input, u32 argument1, u16 *table, u8 *valueOut, u32 options)
{
    Header *source = func_00203664(input, argument1);
    Header *result;
    Header *secondary;
    u32 width;
    u32 height;
    s32 secondaryBytes;
    s32 sourceStride;
    s32 secondaryStride;
    s32 primaryBytes;
    s32 destinationStride;
    u8 *secondaryDestination;
    u16 *destination;
    u8 *indices;
    u8 *primary;
    u8 *auxiliary;
    u32 y;
    u8 secondaryFlags;

    if (source == 0) return 0;
    if (valueOut != 0) *valueOut = ((u8 *)source)[12];
    width = source->width;
    height = source->height;
    destinationStride = ((width + 3) >> 2) * 8;
    primaryBytes = destinationStride * (s32)height;
    if (source->flags & 4) {
        sourceStride = ((width + 7) >> 3) * 8;
        if (source->flags & 1) secondaryStride = ((width + 15) >> 4) * 8;
        else secondaryStride = sourceStride;
    } else {
        sourceStride = destinationStride;
        if (source->flags & 1) secondaryStride = ((width + 15) >> 4) * 8;
        else secondaryStride = ((width + 7) >> 3) * 8;
    }
    secondaryBytes = secondaryStride * (s32)height;
    {
        s32 allocationBytes = secondaryBytes + 16;
        result = resource_alloc(primaryBytes + allocationBytes);
    }
    secondary = (Header *)((u8 *)result + (primaryBytes + 8));
    result->magic0 = 0x36;
    result->magic1 = 0x34;
    result->kind = 0;
    result->flags = 2;
    result->width = width;
    result->height = height;
    secondary->magic0 = 0x36;
    secondary->magic1 = 0x34;
    secondary->kind = 4;
    secondaryFlags = source->flags;
    do {
        secondary->width = width;
    } while (0);
    secondary->height = height;
    secondary->flags = (secondaryFlags ^ 1) & 1;
    destination = (u16 *)(result + 1);
    secondaryDestination = (u8 *)(secondary + 1);
    if (source->flags & 4) {
        indices = primary_data(source);
        memcpy(secondary_data(source), secondaryDestination, secondaryBytes);
        for (y = 0; y < height; y++) {
            u32 remaining = width;
            u16 *destinationRow = destination;
            u8 *sourceRow = indices;
            if (remaining & 1) {
                *destination++ = table[*indices++];
                remaining--;
            }
            if (remaining & 2) {
                *destination++ = table[*indices++];
                *destination++ = table[*indices++];
                remaining -= 2;
            }
            while (remaining != 0) {
                *destination++ = table[*indices++];
                *destination++ = table[*indices++];
                *destination++ = table[*indices++];
                *destination++ = table[*indices++];
                remaining -= 4;
            }
            destination = (u16 *)((u8 *)destinationRow + destinationStride);
            indices = sourceRow + sourceStride;
        }
    } else {
        primary = primary_data(source);
        auxiliary = secondary_data_direct(source);
        do {
            do {
                do {
                    do {
                        do {
                            do {
                                do {
                                    do {
                                        memcpy(primary, destination, primaryBytes);
                                    } while (0);
                                } while (0);
                            } while (0);
                        } while (0);
                    } while (0);
                } while (0);
            } while (0);
        } while (0);
        memcpy(auxiliary, secondaryDestination, secondaryBytes);
    }
    resource_free(source);
    if (options & 1) func_00203030(result);
    if (options & 4) func_002032B8(result);
    return result;
}
