typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
void *memset(void *d, s32 c, u32 n);
void *memset(void *d, s32 c, u32 n) {
    u8 *p = d;
    while (n-- != 0) {
        *p++ = c;
    }
    return d;
}
