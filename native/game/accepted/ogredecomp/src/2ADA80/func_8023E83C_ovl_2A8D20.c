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
    u32 w0;
    u32 w1;
} Gfx;
typedef struct {
               u8 unk00[0x33];
               u8 unk33;
               u8 unk34[2];
               u16 items[4];
} Recruit;
extern u8 *D_8022A974;
extern void *D_801CE8BC;
extern s32 D_801CEAB0;
extern u8 *D_801D06F4;
extern Gfx *D_800E9BA0;
typedef struct {
               u8 cls;
               u8 baseCls;
               u8 unk02[0x36];
} PlayerClass;
extern PlayerClass D_80193BD1[];
void *HMalloc(s32 size);
void *HFree(void *p);
void func_801B0020(void);
void func_801BBC78(void);
void func_801BE894(void);
void func_801C1A9C(void);
void func_801C1D98(void);
void func_801C234C(void);
void func_801C5D64(void);
void func_801CA3E0(Recruit *r);
u8 func_8016FF5C(u8 cls, u8 want);
u16 func_8016DF88(u8 cls, u8 base);
u16 func_8016DFDC(u8 cls, u8 base);
u16 func_8016E030(u8 cls, u8 base);
u16 func_8016E084(u8 cls, u8 base);
void func_8023E320_ovl_2A8D20(void);
void func_8023E354_ovl_2A8D20(void);
void func_8023E7D0_ovl_2A8D20(void);
void func_8023E83C_ovl_2A8D20(u8 *obj) {
    f32 y = (*(f32 *)((u8 *)(obj) + (0x11C))) + 152.0f + (*(f32 *)((u8 *)(obj) + (0x124))) + 38.0f;
    u8 *col = &D_801D06F4[D_801CEAB0 * 6];
    f32 t;
    f32 r;
    f32 g;
    f32 b;
    Gfx *gfx;
    if (y < 0.0f) {
        y = 0.0f;
    } else if (380.0f < y) {
        y = 380.0f;
    }
    t = 380.0f - y;
    r = t * (col[3] - (f32)col[0]) / 380.0f + col[0];
    g = t * (col[4] - (f32)col[1]) / 380.0f + col[1];
    b = t * (col[5] - (f32)col[2]) / 380.0f + col[2];
    gfx = D_800E9BA0++;
    gfx->w0 = 0xFB000000;
    gfx->w1 = ((u32)r << 24) | (((u32)g & 0xFF) << 16) | (((u32)b & 0xFF) << 8) | 1;
}
