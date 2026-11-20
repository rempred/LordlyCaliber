typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    s16 h0;
    s16 h2;
    u8 b4;
    u8 b5;
    u8 b6;
    u8 b7;
    u8 b8;
    u8 b9;
    u8 bA;
    u8 bB;
} StFC10;
extern StFC10 *D_8018FC10;
void *func_00001330(s32 size);
void bzero(void *p, s32 n);
void func_002827EC(s32 a, s32 b);
void func_002838AC(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f) {
    StFC10 *p;
    if (D_8018FC10 == ((void *)0)) {
        D_8018FC10 = func_00001330(0xC);
        bzero(D_8018FC10, 0xC);
        func_002827EC(-2, -1);
    } else if (D_8018FC10->b7 == 1) {
        D_8018FC10->b7 = 0;
        func_002827EC(-2, -1);
    } else {
        D_8018FC10->b7 = 2;
    }
    p = D_8018FC10;
    p->h2 = a;
    p->b4 = b;
    p->b5 = c;
    p->b6 = d;
    p->b9 = f;
    p->h0 = (*(u16 *)((u8 *)(p) + (2)));
    if (e != -1) {
        p->bA = e;
        p->b8 = e;
    } else {
        p->bA = p->b8;
    }
}
