typedef unsigned char u8;
typedef unsigned int u32;
void memmove(u8 *dest, u8 *src, u32 length) {
    if (src < dest) {
        dest += length;
        src += length;
        while (length--) {
            *--dest = *--src;
        }
    } else {
        while (length--) {
            *dest++ = *src++;
        }
    }
}
