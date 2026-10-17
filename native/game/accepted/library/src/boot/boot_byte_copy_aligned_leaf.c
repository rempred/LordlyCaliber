/* Independently reconstructed from the accepted Rev 0 owner at ROM A370. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

void *boot_byte_copy_aligned_leaf(void *destination, const void *source, u32 bytes)
{
    u8 *write;
    const u8 *read;
    u32 words;

    if (bytes != 0) {
        write = destination;
        read = source;
        if (((u32)read - (u32)write) & 3) {
            while (bytes-- != 0) {
                *write++ = *read++;
            }
        } else {
            if ((u32)write & 1) {
                *write++ = *read++;
                bytes--;
            }
            if (bytes >= 2 && ((u32)write & 2)) {
                *(u16 *)write = *(const u16 *)read;
                write += 2;
                read += 2;
                bytes -= 2;
            }
            words = bytes >> 2;
            while (words-- != 0) {
                *(u32 *)write = *(const u32 *)read;
                write += 4;
                read += 4;
            }
            if (bytes & 2) {
                *(u16 *)write = *(const u16 *)read;
                write += 2;
                read += 2;
            }
            if (bytes & 1) {
                *write = *read;
            }
        }
    }
    return destination;
}
