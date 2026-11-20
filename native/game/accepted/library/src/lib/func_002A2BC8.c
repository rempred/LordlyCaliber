typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    s32 id;
    s32 chunk;
    s32 *p08;
    s32 kind;
    u32 n;
    void *p14;
    u8 unk18[4];
    void **list;
    u32 *age;
    u8 *p24;
    s32 w28;
    s32 w2C;
    u8 *w30;
    u8 *p34;
    void *p38;
} Rec2A;
extern Rec2A *D_802395B0;
void D_801BFBA0(void *p);
void func_002A2BC8(void) {
    Rec2A *r;
    u32 i;
    u32 k;
    if (D_802395B0 == ((void *)0)) {
        return;
    }
    for (i = 0; i < 40; i++) {
        r = &D_802395B0[i];
        for (k = 0; k < D_802395B0[i].n; k++) {
            if (r->list[k] != ((void *)0)) {
                D_801BFBA0(r->list[k]);
            }
        }
    }
}
