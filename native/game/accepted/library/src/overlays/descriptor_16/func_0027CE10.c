typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct Task27 {
    u8 unk00[4];
    u16 state;
    u16 next;
    void (*fn)(s32, struct Task27 *);
    s16 hC;
    s16 hE;
    s16 h10;
    u8 unk12[0x26];
} Task27;
extern Task27 *D_8022D148;
extern s16 D_8022D0F2;
void func_0027CE10(void) {
    Task27 *t = D_8022D148;
    s32 i;
    for (i = 0; i < D_8022D0F2; i++) {
        Task27 *p = t;
        if (p->fn != ((void *)0)) {
            if (p->state >= 0x10) {
                p->fn(p->state, t);
            } else if (p->state == 0 && p->hE != -1) {
                if (--p->hE <= 0) {
                    p->state = p->hC;
                }
            }
        }
        t++;
    }
}
