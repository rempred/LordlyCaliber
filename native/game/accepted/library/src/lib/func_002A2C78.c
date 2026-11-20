typedef unsigned char u8;
typedef signed short s16;
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
extern u8 *D_8022A974;
void bzero(void *p, s32 n);
void func_000016C4(void *p);
void func_002A2C78(void) {
    s16 side = (*(s16 *)((u8 *)((*(u8 * *)((u8 *)(D_8022A974) + (0x1A48)))) + (4)));
    u8 *u;
    Rec2A *r;
    s32 id;
    s32 kind;
    u32 i;
    u32 k;
    if (side == -1) {
        return;
    }
    u = (*(u8 * *)((u8 *)(D_8022A974) + (0x18 + side * 4)));
    if (u == ((void *)0)) {
        return;
    }
    id = (*(s32 *)((u8 *)(u) + (0xE8)));
    kind = u[0x146];
    if (D_802395B0 == ((void *)0)) {
        return;
    }
    r = D_802395B0;
    for (i = 0; i < 40; r++, i++) {
        if (r->id == id && r->kind == kind) {
            for (k = 0; k < r->n; k++) {
                if (r->list[k] != ((void *)0)) {
                    func_000016C4(r->list[k]);
                    r->list[k] = ((void *)0);
                }
            }
            func_000016C4(r->age);
            func_000016C4(r->p08);
            func_000016C4((void *)r->w2C);
            func_000016C4((void *)r->w28);
            func_000016C4(r->p38);
            func_000016C4(r->p14);
            bzero(r, 0x3C);
            return;
        }
    }
}
