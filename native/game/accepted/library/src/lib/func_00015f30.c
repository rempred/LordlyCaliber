typedef unsigned char u8;
typedef unsigned int u32;
void func_00015f30(void *dest, u8 value, u32 length);
void func_00015f30(void *dest, u8 value, u32 length) {
    u8 *p = dest;
    while (length--) {
        *p++ = value;
    }
}
