typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
s32 D_801C8B68(u8 *u);
s32 D_801C8BA4(u8 *u);
u16 D_801C8FB8(u8 *u);
void D_801C41C8(s32 *a, s32 *b, s32 *c, s32 *d, s32 *e, s32 f);
void func_002248B0(u8 *u, s32 cls) {
    s32 a;
    s32 b;
    s32 c;
    s32 d;
    s32 e;
    a = (*(s32 *)((u8 *)(u) + (0x48)));
    c = D_801C8B68(u);
    d = D_801C8BA4(u);
    b = cls;
    e = D_801C8FB8(u);
    D_801C41C8(&a, &b, &c, &d, &e, 1);
}
