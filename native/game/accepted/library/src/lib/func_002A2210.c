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
void func_000016C4(void *p);
void func_002A2210(s32 keep) {
    u32 i;
    u32 k;
    if (D_802395B0 == ((void *)0)) {
        return;
    }
    for (i = 0; i < 40; i++) {
        for (k = 0; k < D_802395B0[i].n; k++) {
            if (D_802395B0[i].list[k] != ((void *)0)) {
                func_000016C4(D_802395B0[i].list[k]);
            }
        }
        func_000016C4(D_802395B0[i].age);
        func_000016C4(D_802395B0[i].p08);
        func_000016C4((void *)D_802395B0[i].w2C);
        func_000016C4((void *)D_802395B0[i].w28);
        func_000016C4(D_802395B0[i].p38);
        func_000016C4(D_802395B0[i].p14);
    }
    if (!keep) {
        func_000016C4(D_802395B0);
        D_802395B0 = ((void *)0);
    }
}
