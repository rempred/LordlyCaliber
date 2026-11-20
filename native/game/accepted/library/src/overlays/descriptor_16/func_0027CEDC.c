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
Task27 *func_0027CEDC(void (*fn)(s32, Task27 *)) {
    Task27 *t = ((void *)0);
    if (D_8022D0F0 != 0xFFFF) {
        t = &D_8022D148[D_8022D0F0];
        D_8022D0F0 = t->next;
    }
    if (t != ((void *)0)) {
        t->fn = fn;
        t->h10 = 0;
        fn(0x10, t);
    }
    return t;
}
