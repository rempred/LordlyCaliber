typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    s16 h0;
    u8 unk02[2];
    s32 w4;
    u8 unk08[4];
    s32 wC;
    u8 unk10[0x74];
    s32 x84;
    u8 x88;
    u8 x89;
    u8 x8A;
    u8 x8B;
} Widget3390;
void func_000016C4(void *p);
void func_000016C4(void *p);
void bzero(void *p, s32 n);
void func_000fbc80(Widget3390 *p);
void *func_00001330(s32 size);
void *D_8009DBB8(void *dst, s32 id);
s32 func_0000ABE0(void *p);
void func_0000A510(void *dst, void *src);
void func_000fbc80(Widget3390 *o) {
    u32 i;
    s32 *tbl;
    u8 *raw;
    void *img;
    bzero((u8 *)o + 0x19C, 0x48);
    for (i = 0; i < 18; i++) {
        tbl = D_8009DBB8(((void *)0), 0x1D61984);
        raw = D_8009DBB8(((void *)0), tbl[i]);
        img = func_00001330(func_0000ABE0(raw));
        func_0000A510(img, raw);
        func_000016C4(raw);
        func_000016C4(tbl);
        ((void **)((u8 *)o + 0x19C))[i] = img;
    }
    bzero((u8 *)o + 0x258, 0x17C);
    bzero((u8 *)o + 0x3D4, 0x70);
    bzero((u8 *)o + 0x444, 0x70);
    bzero((u8 *)o + 0x4B4, 0x70);
    bzero((u8 *)o + 0x524, 0xC);
}
