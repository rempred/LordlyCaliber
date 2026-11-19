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
               u16 flags;
               u8 flags2;
               u8 flags3;
               u8 unk04[2];
               s16 x;
               s16 y;
               s16 x2;
               s16 y2;
               u16 unk0E;
               void *owner;
               u8 unk14[0x14];
               s16 unk28;
               s16 unk2A;
               u8 unk2C[0xC];
               void *slotsA[10];
               void *slotsB[10];
               u8 unk88;
               u8 unk89;
               u8 unk8A;
               u8 unk8B;
               u8 unk8C;
               u8 unk8D;
               u8 unk8E;
               u8 unk8F;
               u8 unk90[4];
               u8 unk94;
               u8 unk95;
               u8 unk96;
               u8 unk97;
               u8 unk98[0xA];
               s16 parent;
               u8 unkA4[4];
} Scene;
extern Scene D_800E82C8[];
typedef struct {
               f32 persp[5];
               f32 eye[3];
               f32 at[3];
               f32 up[3];
               s32 rest[8];
} Camera;
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
typedef struct {
               void *p0;
               void *p1;
               u8 (*mtx)[0x40];
} FbSet;
typedef struct {
               Camera *cam;
               Camera *saved;
} CamSave;
typedef struct {
               void *image;
} TexRef;
typedef struct {
               s16 unk00;
               CamSave cs;
               TexRef tex;
               s16 unk10;
               s16 unk12;
               s16 unk14;
               s16 unk16;
               s16 unk18;
               f32 scaleX;
               f32 scaleY;
               u8 unk24[0xC];
               void *unk30;
               u8 unk34[6];
               s16 alpha;
               f32 unk3C;
               f32 unk40;
               u8 unk44[0x10];
} FadeCtx;
extern u16 *D_801AB850;
extern u8 D_801F365D;
extern s32 D_8018F590;
extern u16 *D_800C4BDC;
extern u16 *D_801F0CA0;
extern s32 D_801F0DE0;
extern s32 D_801F0DE8;
extern u8 D_800E7A90[];
extern FbSet *D_800E9BE0;
extern Gfx *D_800E9BA0;
extern u16 D_800C4BE4;
extern u16 D_800C4C24;
extern u16 D_800E7A0E;
extern u8 D_80216F18_ovl_1A2C20[];
extern u8 D_80216BF0_ovl_1A2C20[];
extern void *D_80216EE8_ovl_1A2C20[];
extern u8 D_80216C70_ovl_1A2C20[];
extern u8 D_80216CF0_ovl_1A2C20[];
extern u8 D_80216D70_ovl_1A2C20[];
extern u8 D_800EB0B0[];
extern Camera *D_8018F58C;
extern u8 D_8018F53C;
extern s32 D_8018F548;
extern s32 D_8018F54C;
extern u8 D_8018F550;
extern u8 D_8018F551;
extern u8 D_8018F552;
extern u8 D_802167E0_ovl_1A2C20[];
extern FadeCtx *D_80216BE0_ovl_1A2C20;
extern u8 D_80216BE4_ovl_1A2C20;
extern s32 D_80216BE8_ovl_1A2C20;
extern s32 D_80216EF8_ovl_1A2C20;
extern s32 D_80216EFC_ovl_1A2C20;
extern s32 D_80216F00_ovl_1A2C20;
extern u16 D_80216F70_ovl_1A2C20;
extern s32 D_80216F74_ovl_1A2C20;
extern s32 D_80216F78_ovl_1A2C20;
extern u8 D_80216F7C_ovl_1A2C20;
extern u8 D_80216F7D_ovl_1A2C20;
extern u8 D_80216F7E_ovl_1A2C20;
extern s32 D_80216F80_ovl_1A2C20;
extern s32 D_80216F84_ovl_1A2C20;
extern s8 D_80216F88_ovl_1A2C20;
extern s8 D_80216F89_ovl_1A2C20;
extern s8 D_80216F8A_ovl_1A2C20;
extern s16 D_80216F8C_ovl_1A2C20;
void *HMalloc(s32 size);
void *HFree(void *p);
void bzero(void *p, s32 n);
void *LoadAsset(u32 handle, ...);
void guOrtho(void *m, f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale);
void func_80098B60(f32 mf[4][4], f32 x, f32 y, f32 z);
void func_80098D00(f32 mf[4][4], f32 x, f32 y, f32 z);
void func_800987E0(void *m);
void guMtxCatF(f32 a[4][4], f32 b[4][4], f32 out[4][4]);
void guMtxF2L(f32 mf[4][4], void *m);
void guLookAt(void *m, f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp);
s16 func_801AB0FC(u16 a, s32 b);
void func_800EA6B0(void *ch, s32 a, s32 b, s32 c);
void func_800EA7E4(void *ch, s32 v);
void func_80215B1C_ovl_1A2C20(void);
void func_8017BED4(void);
void func_801C9AA4(void);
void func_80215D30_ovl_1A2C20(void);
void func_8021608C_ovl_1A2C20(void);
extern inline void func_80214F80_ovl_1A2C20(void) {
    D_80216F8C_ovl_1A2C20 = func_801AB0FC(*D_801AB850, 0x3C);
    D_800E82C8[D_80216F8C_ovl_1A2C20].unk89 = 9;
    D_80216F70_ovl_1A2C20 |= 0x20;
    func_800EA7E4(D_800EB0B0, 300);
}
void func_80215000_ovl_1A2C20(u8 r, u8 g, u8 b, s32 frames);
s32 func_80215298_ovl_1A2C20(void);
extern inline void func_8021562C_ovl_1A2C20(void) {
    CamSave *cs = &D_80216BE0_ovl_1A2C20->cs;
    *cs->cam = *cs->saved;
    HFree(cs->saved);
    HFree(D_80216BE0_ovl_1A2C20);
    D_80216BE0_ovl_1A2C20 = ((void *)0);
    D_80216BE4_ovl_1A2C20 = 0;
}
extern inline s32 func_802156B8_ovl_1A2C20(void) {
    s32 t;
    f32 sx;
    f32 sy;
    if (!(D_80216F70_ovl_1A2C20 & 1)) {
        return 0;
    }
    t = D_80216EF8_ovl_1A2C20;
    sx = 1.45 - t / 100.0f;
    sy = t / 45.0f;
    if (sx < 1.0f) {
        sx = 1.0f;
    }
    if (1.0f < sy) {
        sy = 1.0f;
    }
    D_80216EF8_ovl_1A2C20 = t + 1;
    D_80216BE0_ovl_1A2C20->scaleX = sx;
    D_80216BE0_ovl_1A2C20->scaleY = sy;
    return 0;
}
extern inline s32 func_80215764_ovl_1A2C20(void) {
    s32 t;
    f32 scale;
    if (!(D_80216F70_ovl_1A2C20 & 2)) {
        return 0;
    }
    if (D_80216EFC_ovl_1A2C20 >= 120) {
        D_80216EFC_ovl_1A2C20 = 120;
    }
    t = D_80216EFC_ovl_1A2C20;
    scale = 1.2 - t / 100.0f;
    D_80216EFC_ovl_1A2C20 = t + 1;
    D_80216BE0_ovl_1A2C20->unk3C = scale;
    D_80216BE0_ovl_1A2C20->unk40 = scale;
    return 0;
}
void func_802157F0_ovl_1A2C20(void);
void func_80215AC8_ovl_1A2C20(void);
void func_80215B1C_ovl_1A2C20(void);
void func_80215D30_ovl_1A2C20(void);
void func_8021608C_ovl_1A2C20(void);
void func_80216380_ovl_1A2C20(void);
u8 func_802163AC_ovl_1A2C20(void);
s32 func_802163B8_ovl_1A2C20(void) {
    s32 state;
    s32 ret = 0;
    D_801F365D |= 1;
    state = D_80216BE4_ovl_1A2C20;
    if (state == 0) {
        D_8018F590 = 1;
        func_8017BED4();
        func_802154E0_ovl_1A2C20();
    }
    if ((*D_800C4BDC & D_801F0CA0[0]) || (*D_800C4BDC & D_801F0CA0[1])) {
        if (state == 4) {
            func_80214F80_ovl_1A2C20();
        }
    }
    if (state == 1) {
        func_8021562C_ovl_1A2C20();
        ret = -3;
        D_801F0DE0 = 0;
        D_801F0DE8 = 0;
        D_801F365D &= ~1;
        D_800E82C8[D_800E7A90[0x29]].unk8B &= ~4;
        func_801C9AA4();
        D_8018F590 = 0;
    } else {
        func_802157F0_ovl_1A2C20();
    }
    return ret;
}
