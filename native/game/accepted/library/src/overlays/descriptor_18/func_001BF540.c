typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
void func_001BF540(u8 *p, s32 a) {
    u32 i;
    (*(s32 *)((u8 *)(p) + (0x18))) = a;
    (*(s32 *)((u8 *)(p) + (0x1C))) = 0;
    p[0x11] = 0;
    (*(s32 *)((u8 *)(p) + (0x38))) = 0;
    p[0x22] = 0;
    for (i = 0; i < 4; i++) {
        (*(s32 *)((u8 *)(p) + (0x24 + i * 4))) = 0;
    }
    p[0x41] = 0;
    p[0x40] = 0;
    if (p[0xD] != 0 && p[0xD] < 17) {
        p[0xD] = 1;
    }
}
