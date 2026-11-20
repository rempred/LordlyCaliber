typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct Task23 {
    u16 next;
    u8 unk02[2];
    void (*fn)(s32, struct Task23 *);
    u16 state;
    u16 nextState;
    s16 hC;
    s16 hE;
    u8 unk10[0x20];
} Task23;
extern Task23 *D_801F0560;
extern s32 D_801F0538;
void func_0023B4BC(void) {
    Task23 *t = D_801F0560;
    s32 i;
    for (i = 0; i < D_801F0538; t++, i++) {
        if (t->fn == ((void *)0)) {
            continue;
        }
        if (t->state >= 0x10) {
            t->fn(t->state, t);
            continue;
        }
        if (t->state != 0) {
            continue;
        }
        if (t->hC == -1) {
            continue;
        }
        if (--t->hC > 0) {
            continue;
        }
        t->state = t->nextState;
    }
}
