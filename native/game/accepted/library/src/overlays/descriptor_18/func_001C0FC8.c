typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
s32 D_8016FBE0(u8 a, u8 b, void *out);
void func_001C0FC8(u8 *p, s32 a, s32 b, s32 c) {
    p[0xF] |= c;
    (*(s32 *)((u8 *)(p) + (0x4C))) = D_8016FBE0(a, b, p + 0x50);
    (*(u16 *)((u8 *)(p) + (0x46))) = 2;
    (*(u16 *)((u8 *)(p) + (0x14))) = 2;
    (*(u16 *)((u8 *)(p) + (0x48))) = 2;
    (*(u16 *)((u8 *)(p) + (0x20))) = (*(u16 *)((u8 *)(p) + (0x46))) + 0x2C;
}
