typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
extern u8 *D_8022A978;
extern u8 D_8022A980;
void *func_00001330(s32 size);
extern u8 D_8022A980;
void func_002AD6D0(s32 w, s32 h, s32 x, s32 y, s32 a, s32 b) {
    D_8022A980 = 0x80;
    if (D_8022A978 == ((void *)0)) {
        D_8022A978 = func_00001330(0xE);
    }
    if (w == -1 && h == -1) {
        ((s16 *)D_8022A978)[2] = 24;
        ((s16 *)D_8022A978)[3] = 24;
    } else {
        ((s16 *)D_8022A978)[2] = w;
        ((s16 *)D_8022A978)[3] = h;
    }
    {
        s16 *p = (s16 *)D_8022A978;
        p[0] = x;
        p[1] = y;
        p[4] = 0;
        p[5] = a;
        p[6] = b;
    }
}
