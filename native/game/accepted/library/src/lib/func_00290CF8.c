typedef unsigned char u8;
typedef signed short s16;
typedef struct {
    u8 unk0[8];
    s16 a;
    s16 b;
} Pair28;
extern Pair28 *D_8023DE38;
void *func_000016C4(void *p);
typedef struct {
    s16 h0;
    s16 h2;
    s16 h4;
    s16 h6;
    s16 h8;
    s16 hA;
    s16 hC;
} Pair28Full;
void func_00290CF8(void) {
    Pair28Full *p = (Pair28Full *)D_8023DE38;
    if (p != ((void *)0)) {
        if (p->h8 == p->hA) {
            if (p->hC == 0) {
                func_000016C4(p);
                D_8023DE38 = ((void *)0);
            }
        } else {
            p->h8++;
        }
    }
}
