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
extern u16 D_8022D0F0;
extern Task27 *D_8022D148;
void func_0027CF50(Task27 *t) {
    t->fn = ((void *)0);
    t->h10 = 0;
    t->next = D_8022D0F0;
    D_8022D0F0 = t - D_8022D148;
}
