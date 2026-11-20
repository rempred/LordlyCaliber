typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
void func_001C1D00(s32 id);
void D_8017C4B4(s16 id);
void func_001BDF84(s16 x0, s16 y0, s16 x1, s16 y1);
void D_80179034(void *s, s32 x, s32 y);
extern u8 D_8022A1D8[];
typedef struct {
    s32 used;
    void *text;
    u8 flags;
    u8 state;
    s16 hA;
    s16 hC;
    u16 hE;
    u16 h10;
    u16 h12;
    u16 h14;
    s16 h16;
    s16 h18;
    s16 h1A;
    s16 h1C;
    u8 unk1E[2];
} Rec698;
extern Rec698 D_8022A698[];
extern inline Rec698 *Rec698Find(s32 id) {
    Rec698 *r = ((void *)0);
    s32 i;
    for (i = 0; i < 4; i++) {
        if (D_8022A698[i].used != 0 && D_8022A698[i].used == id) {
            r = &D_8022A698[i];
            break;
        }
    }
    return r;
}
void func_001C1D00(s32 id) {
    Rec698 *r = Rec698Find(id);
    u16 x;
    if (r == ((void *)0)) {
        for (;;);
    }
    D_8017C4B4((*(s16 *)((u8 *)(r) + (0x16))));
    x = (*(u16 *)((u8 *)(r) + (0xA))) - (*(u16 *)((u8 *)(r) + (0x10))) / 2;
    func_001BDF84(x, (*(u16 *)((u8 *)(r) + (0xC))) - 11, x + (*(u16 *)((u8 *)(r) + (0x10))), (*(u16 *)((u8 *)(r) + (0xC))) + 10);
    D_80179034(D_8022A1D8, 0, 0);
    D_80179034((*(void * *)((u8 *)(r) + (4))), (*(s16 *)((u8 *)(r) + (0xA))) - (*(u16 *)((u8 *)(r) + (0xE))) / 2 + 8, (*(s16 *)((u8 *)(r) + (0xC))) - 8);
}
