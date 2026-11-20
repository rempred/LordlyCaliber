typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
void bzero(void *p, s32 n);
void func_0029D468(u16 *p, s16 a, s16 b, s32 c) {
    bzero(p, 0x10);
    p[0] = a;
    p[1] = b;
    ((u8 *)p)[0xC] = 0;
    ((u8 *)p)[0xD] = 0;
    ((u8 *)p)[0xE] = 0;
    ((u8 *)p)[0xF] = c;
}
