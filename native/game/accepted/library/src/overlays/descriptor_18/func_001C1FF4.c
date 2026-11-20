typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
void *D_8017F334(u16 id);
void D_8017C2FC(void *p);
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
void func_000016C4(void *p);
extern void *D_8022A654;
void func_001C1FF4(void) {
    s32 i;
    void *o;
    for (i = 0; i < 4; i++) {
        o = D_8017F334(i | 0x5400);
        if (o != ((void *)0)) {
            D_8017C2FC(o);
        }
        D_8022A698[i].used = 0;
    }
    func_000016C4(D_8022A654);
}
