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
    u32 w[0x30];
} Unit105;
extern Unit105 D_801F38D8_ovl_101CE0;
void _bcopy(void *src, void *dst, s32 n);
void func_801B14E0_ovl_101CE0(Unit105 *u);
void func_801B1538_ovl_101CE0(Unit105 *u) {
    u32 keep = u->w[0x2A];
    _bcopy(&D_801F38D8_ovl_101CE0, u, sizeof(Unit105));
    u->w[0x2A] = keep;
}
