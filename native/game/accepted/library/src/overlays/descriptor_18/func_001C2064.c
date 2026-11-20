typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
void bzero(void *p, s32 size);
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
extern u8 D_8022A658[];
void *func_00001330(s32 size);
void *D_8009DD38(s32 id);
void bcopy(void *src, void *dst, s32 n);
typedef struct { u8 b; u8 pad[31]; } Rec32;
extern Rec32 D_8022A6A1[];
static void func_001C20F8(s32 i);
static s32 func_001C2150(s32 i);
void func_001C2064(void) {
    u8 *p;
    bzero(D_8022A698, 0x80);
    D_8022A654 = func_00001330(0x2D0);
    p = D_8009DD38(0x1DC873E);
    bcopy(p + 0x3E28, D_8022A654, 0x2D0);
    func_000016C4(p);
    p = D_8009DD38(0x1DCB92C);
    bcopy(p + 0x520, D_8022A658, 0x20);
    func_000016C4(p);
}
static void func_001C20F8(s32 i) {
    Rec698 *r;
    if (i >= 4) {
        for (;;) {
        }
    }
    r = &D_8022A698[i];
    if (r->used == 0) {
        for (;;) {
        }
    }
    r->h14 = 0;
    r->h1A = 0;
    r->h1C = 0;
    r->state = 2;
    r->h12 = r->h10;
    r->h18 = r->h16;
}
static s32 func_001C2150(s32 i) {
    return D_8022A6A1[i].b == 3;
}
