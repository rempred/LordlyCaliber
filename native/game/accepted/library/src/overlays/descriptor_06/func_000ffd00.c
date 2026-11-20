typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
void func_000ffd00(u8 *p, s16 a, u16 *x, u16 *y) {
    s32 i;
    for (i = 0; i < 4; i++) {
        (*(u16 *)((u8 *)(p) + (0x94 + i * 2))) = *x++;
        (*(u16 *)((u8 *)(p) + (0x9C + i * 2))) = *y++;
    }
    (*(s16 *)((u8 *)(p) + (0x88))) = 0;
    (*(s16 *)((u8 *)(p) + (0x8A))) = a;
}
