typedef unsigned char u8;
u8 *strcpy(u8 *d, u8 *s);
u8 *strcpy(u8 *d, u8 *s) {
    u8 *p = d;
    while (*s != 0) {
        *p++ = *s++;
    }
    *p = 0;
    return d;
}
