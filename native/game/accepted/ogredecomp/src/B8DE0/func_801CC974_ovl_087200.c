typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile s32 vs32;
typedef struct {
                s32 f00;
                s32 f04;
                s32 f08;
                s32 cur[4];
                s32 v1C;
                s32 v20;
                s32 v24;
                s32 v28;
                s32 start[4];
                u8 unk03C[0xAA];
                u8 flags;
                u8 unk0E7;
                u16 mode;
                u16 type;
                u16 unk0EC;
                u16 id;
                s32 unk0F0;
                void *data;
                u8 unk0F8[0x10];
} AnimSlot;
typedef struct {
                 u8 unk0000[0x650];
                 AnimSlot slots[10];
                 u16 count;
} AnimState;
extern AnimState *D_80196AF8;
void *HFree(void *p);
void _bcopy(void *src, void *dst, s32 n);
void func_801CC84C_ovl_087200(s16 i);
void func_801CC380_ovl_087200(void);
extern inline s16 func_801CC3B8_ovl_087200(u16 id) {
    AnimSlot *s;
    s32 i;
    for (i = 0; i < 10; i++) {
        s = &D_80196AF8->slots[i];
        if ((s->flags & 1) && s->id == id) {
            return i;
        }
    }
    return -1;
}
AnimSlot *func_801CC410_ovl_087200(u16 id);
AnimSlot *func_801CC464_ovl_087200(void);
void func_801CC4A0_ovl_087200(AnimSlot *s);
extern inline s16 FindFreeSlot(void) {
    AnimState *st = D_80196AF8;
    AnimSlot *s;
    s32 i;
    for (i = 0; i < 10; i++) {
        s = &st->slots[i];
        if (s->flags == 0) {
            return i;
        }
    }
    return -1;
}
void func_801CC50C_ovl_087200(AnimSlot *src);
void func_801CC5B4_ovl_087200(AnimSlot *s, s16 a, s16 b, s16 c, s16 d);
void func_801CC5EC_ovl_087200(AnimSlot *s, s16 a, s16 b, s16 c, s16 d, s16 e, s16 f, s16 g, s16 h);
void func_801CC684_ovl_087200(AnimSlot *s, s32 a, s16 b, s16 c, s16 d, s16 e);
void func_801CC6D4_ovl_087200(AnimSlot *s, s32 a, s32 b, s16 c, s16 d, s16 e, s16 f);
void func_801CC72C_ovl_087200(AnimSlot *s, s32 a, s32 b, s16 c, s16 d, s16 e, s16 f);
void func_801CC784_ovl_087200(AnimSlot *s, s16 a, s16 b, s16 c, s16 d);
void func_801CC7D4_ovl_087200(u16 id);
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    u16 flag;
    s16 s;
    s16 t;
    u8 rgba[4];
} VtxB8;
s32 func_801CCA50_ovl_087200(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h);
void func_801CC84C_ovl_087200(s16 i);
s32 func_801CC974_ovl_087200(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h, s16 x2, s16 y2, s16 w2, s16 h2) {
    s32 i = func_801CCA50_ovl_087200(kind, b, fn, x, y, w, h);
    AnimSlot *e = &D_80196AF8->slots[i];
    e->type = 1;
    e->v1C = x2;
    e->v20 = y2;
    e->v24 = w2;
    e->v28 = h2;
    e->mode = 0x8000;
    return i;
}
void func_801CCD08_ovl_087200(VtxB8 *v, s16 x0, s16 y0, s16 x1, s16 y1);
