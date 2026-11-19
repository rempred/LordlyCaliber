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
    s32 data[0x16];
} Camera;
typedef struct {
               Camera *saved;
               s32 arg;
               s16 timer;
               s16 alpha;
               s16 *alphaPtr;
               s16 unk10;
               s32 unk14;
} Ovl17F9C0Ctx;
extern Ovl17F9C0Ctx *D_8021AF00_ovl_17F9C0;
extern u8 D_8021AF04_ovl_17F9C0;
extern Camera *D_8018F58C;
extern u8 D_800EB290[];
extern u8 D_801F365D;
extern u8 D_800E7A90[];
extern s32 D_801F3620;
void *HMalloc(s32 size);
void *HFree(void *p);
void func_800EA604(void *ch, s32 sound);
s32 func_80216AE0_ovl_17F9C0(void);
void func_80216F3C_ovl_17F9C0(void);
void func_80217114_ovl_17F9C0(void);
void func_801DFF50(void);
void func_801DFF80(void);
void func_8017BED4(void);
extern inline void func_80217474_ovl_17F9C0(s32 arg) {
    D_8021AF00_ovl_17F9C0 = HMalloc(sizeof(Ovl17F9C0Ctx));
    D_8021AF00_ovl_17F9C0->arg = arg;
    D_8021AF00_ovl_17F9C0->saved = HMalloc(sizeof(Camera));
    *D_8021AF00_ovl_17F9C0->saved = *D_8018F58C;
    D_8021AF00_ovl_17F9C0->timer = 0;
    D_8021AF00_ovl_17F9C0->alpha = 0xFF;
}
extern inline void func_8021751C_ovl_17F9C0(void) {
    *D_8018F58C = *D_8021AF00_ovl_17F9C0->saved;
    HFree(D_8021AF00_ovl_17F9C0->saved);
    HFree(D_8021AF00_ovl_17F9C0);
    D_8021AF00_ovl_17F9C0 = ((void *)0);
    D_8021AF04_ovl_17F9C0 = 0;
}
extern inline void func_802175B0_ovl_17F9C0(void) {
    Ovl17F9C0Ctx *ctx = D_8021AF00_ovl_17F9C0;
    if (ctx->timer == 0) {
        func_800EA604(D_800EB290, 0x288);
    }
    D_8021AF00_ovl_17F9C0->timer++;
    ctx->alphaPtr = &D_8021AF00_ovl_17F9C0->alpha;
    ctx->unk10 = 0;
    ctx->unk14 = 0x26;
    func_80216AE0_ovl_17F9C0();
    if (D_8021AF00_ovl_17F9C0->timer < 50) {
        D_8021AF04_ovl_17F9C0 = 4;
    } else {
        D_8021AF04_ovl_17F9C0 = 1;
    }
}
void func_80217640_ovl_17F9C0(void);
u8 func_80217664_ovl_17F9C0(void);
s32 func_80217670_ovl_17F9C0(void) {
    s32 ret = 0;
    D_801F365D |= 1;
    D_800E82C8[D_800E7A90[0x29]].unk8B |= 4;
    if (D_8021AF04_ovl_17F9C0 == 0) {
        func_801DFF50();
        func_8017BED4();
        func_80217474_ovl_17F9C0(D_801F3620);
    }
    func_802175B0_ovl_17F9C0();
    if (D_8021AF04_ovl_17F9C0 == 1) {
        func_8021751C_ovl_17F9C0();
        ret = 1;
        func_801DFF80();
    }
    return ret;
}
