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
    u8 unk0[0xC];
    void *data;
} Res2A3;
typedef struct {
    u8 unk0000[0x1CA4];
    Res2A3 *res;
} Battle2A3;
extern Battle2A3 *D_8022A974;
void *HFree(void *p);
void func_8022B6F4_ovl_29A4C0(void);
void func_80233398_ovl_29A4C0(void);
extern u8 *D_80197AF0;
void nuGfxTaskAllEndWait(void);
void *osViGetCurrentFramebuffer(void);
void func_80080998(void *dst, void *src, s32 n);
void func_80234E50_ovl_29A4C0(void);
extern u8 D_8018FC19;
extern s32 D_80197774;
extern u8 D_8022A960;
void func_801C22E8(s32 a);
void func_801C45E0(void);
void func_801B7E9C(void);
void func_8023AA84(s32 a);
void func_8023BF30(u8 a, s32 b);
void func_80233580_ovl_29A4C0(void);
void func_8022BDDC_ovl_29A4C0(void);
s32 func_80234770_ovl_29A4C0(void);
void func_80234BE0_ovl_29A4C0(void);
void func_80234C04_ovl_29A4C0(void);
void func_80234E50_ovl_29A4C0(void) {
    Res2A3 *r = D_8022A974->res;
    if (r != ((void *)0)) {
        HFree(r->data);
        HFree(r);
        D_8022A974->res = ((void *)0);
    }
}
