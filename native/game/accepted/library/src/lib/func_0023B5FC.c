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
void func_0023B5FC(Task23 *t) {
    t->fn = ((void *)0);
    t->hE = 0;
    t->next = D_801F0534;
    D_801F0534 = t - D_801F0560;
}
