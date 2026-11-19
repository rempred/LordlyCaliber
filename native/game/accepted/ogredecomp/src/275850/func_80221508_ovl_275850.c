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
               u8 unk00[0x1C];
               void *w1C;
               u16 flags;
               s16 h22;
               s32 w24;
               s16 h28;
} Obj27;
typedef struct {
              u8 unk0[4];
              s32 x;
              s32 y;
              s32 z;
} IPos27;
typedef struct {
    f32 x, y, z;
} Vec3f;
void *func_8020C9B0(s32 a);
void func_8020C074(void *p);
typedef struct {
    u32 w0;
    u32 w1;
} Gfx;
extern Gfx *D_800E9BA0;
typedef struct {
    s32 m[16];
} Mtx40;
extern Mtx40 *D_801969B0;
extern Gfx D_80225620_ovl_275850[];
void func_80098D00(f32 mf[4][4], f32 x, f32 y, f32 z);
void guMtxF2L(f32 mf[4][4], void *m);
s32 func_802210C0_ovl_275850(u8 *o, s32 k);
void func_80221368_ovl_275850(u8 *o, s32 a);
void func_80221508_ovl_275850(u8 *o) {
    f32 m[4][4];
    Gfx *g;
    func_80098D00(m, 0.0f, 0.0f, 0.0f);
    guMtxF2L(m, D_801969B0);
    g = D_800E9BA0++;
    g->w0 = 0xDA380000;
    g->w1 = (u32)D_801969B0++;
    g = D_800E9BA0++;
    g->w0 = 0xDE000000;
    g->w1 = (u32)D_80225620_ovl_275850;
    func_80221368_ovl_275850(o, func_802210C0_ovl_275850(o, 0));
    func_80221368_ovl_275850(o, func_802210C0_ovl_275850(o, 1));
    g = D_800E9BA0++;
    g->w0 = 0xD8380002;
    g->w1 = 0x40;
}
void func_802215FC_ovl_275850(Obj27 *o, s32 v);
void func_80221610_ovl_275850(Obj27 *o, f32 v);
void func_8022162C_ovl_275850(Obj27 *o, s16 v);
void func_80221634_ovl_275850(IPos27 *out, Vec3f *a, Vec3f *b);
void func_802216AC_ovl_275850(Obj27 *o);
void func_802216BC_ovl_275850(Obj27 *o);
s32 func_802216CC_ovl_275850(Obj27 *o);
void func_802216D8_ovl_275850(Obj27 *o);
void func_80221720_ovl_275850(Obj27 *o);
void func_80221748_ovl_275850(void);
