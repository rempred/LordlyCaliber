typedef unsigned char u8;
typedef signed int s32;
void func_000573ac(u8 *obj, s32 i, u8 *dst, u8 n) {
    s32 pad;
    u8 *src = (*(u8 * *)((u8 *)((*(u8 * *)((u8 *)(obj) + (0xD0))) + i * 4) + (0x530)));
    s32 k;
    for (k = 0; k < n; k++) {
        *dst++ = *src++;
        *dst++ = *src++;
    }
    *dst = 0;
}
