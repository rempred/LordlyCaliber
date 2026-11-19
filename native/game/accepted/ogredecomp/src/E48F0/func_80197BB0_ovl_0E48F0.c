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
extern Gfx *D_800E9BA0;
extern s32 D_8019E180_ovl_0E48F0;
extern u8 D_8018F480;
extern u8 D_80186BD0[];
extern u8 D_80186D90[];
void func_80197B70_ovl_0E48F0(void);
void func_80197BB0_ovl_0E48F0(void) {
    Gfx *g = D_800E9BA0++;
    g->w0 = 0xDE000000;
    g->w1 = (u32)D_80186D90;
}
