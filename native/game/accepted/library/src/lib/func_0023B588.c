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
extern u16 D_801F0534;
extern Task23 *D_801F0560;
Task23 *func_0023B588(void (*fn)(s32, Task23 *)) {
    Task23 *t = ((void *)0);
    if (D_801F0534 != 0xFFFF) {
        t = &D_801F0560[D_801F0534];
        D_801F0534 = t->next;
    }
    if (t != ((void *)0)) {
        t->fn = fn;
        t->hE = 0;
        fn(0x10, t);
    }
    return t;
}
