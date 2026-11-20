typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef float f32;
typedef struct {
    u16 active;
    u8 pad02[2];
    s32 a;
    f32 x;
    f32 y;
    f32 f10;
    f32 f14;
    s16 f18;
    s16 f1A;
    u16 f1C[4];
    u8 pad24[8];
    u16 f2C[4];
    s32 f34;
    s32 f38;
    s32 f3C;
    s32 f40;
} Anim0444;
void func_000fd484(Anim0444 *p, s32 a, f32 x, f32 y, u16 *src) {
    s32 i;
    for (i = 0; i < 4; i++) {
        p->f1C[i] = p->f2C[i] = src[i];
    }
    p->a = a;
    p->x = x;
    p->y = y;
    p->f1A = 0;
    p->f18 = 0;
    p->f34 = 0;
    p->f3C = 0;
    p->f38 = 0;
    p->f40 = 0;
    p->active = 1;
    p->f14 = 1.0f;
    p->f10 = 1.0f;
}
