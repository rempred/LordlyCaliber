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
extern void *D_801F0530;
extern u16 D_801F0534;
extern Task23 *D_801F0560;
extern s32 D_801F0538;
void *func_00001330(s32 size);
void bzero(void *p, s32 n);
void func_0023B3E0(void) {
    Task23 *p;
    s32 i;
    D_801F0538 = 3;
    D_801F0534 = 0;
    D_801F0530 = func_00001330(0x90);
    bzero(D_801F0530, 0x90);
    D_801F0560 = D_801F0530;
    for (i = 0; i < D_801F0538; i++) {
        D_801F0560[i].fn = ((void *)0);
    }
    p = D_801F0560;
    for (i = 1; i < D_801F0538; i++) {
        p->next = i;
        p++;
    }
    p->next = 0xFFFF;
}
