typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct {
    u8 unk0[8];
    s16 a;
    s16 b;
} Pair28;
extern Pair28 *D_8023DE38;
s32 func_002910DC(void) {
    Pair28 *p = D_8023DE38;
    s32 r = 0;
    if (p != ((void *)0)) {
        r = p->a != p->b;
    }
    return r;
}
