/* Independently reconstructed from the accepted Rev 0 owner at ROM A470. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

void *boot_byte_fill_aligned_leaf(void *destination, int value, u32 bytes)
{
    u8 *write;
    u32 words;

    if (bytes != 0) {
        value &= 0xff;
        value |= value << 8;
        value |= value << 16;
        write = destination;
        if ((u32)write & 1) {
            *write++ = value;
            bytes--;
        }
        if (bytes >= 2 && ((u32)write & 2)) {
            *(u16 *)write = value;
            write += 2;
            bytes -= 2;
        }
        words = bytes >> 2;
        while (words-- != 0) {
            *(u32 *)write = value;
            write += 4;
        }
        if (bytes & 2) {
            *(u16 *)write = value;
            write += 2;
        }
        if (bytes & 1) {
            *write = value;
        }
    }
    return destination;
}
