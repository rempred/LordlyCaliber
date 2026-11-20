typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u8 *D_80196AF8;
void bcopy(const void *src, void *dst, s32 n);
void func_000b84d4(u16 i) {
    bcopy(D_80196AF8 + 0x1806, D_80196AF8 + 0x117C + i * 0x36, 0x36);
}
