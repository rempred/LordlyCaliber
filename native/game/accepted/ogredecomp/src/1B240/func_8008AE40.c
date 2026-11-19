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
extern u16 D_800AA010;
extern u32 D_800AA014;
void _bcopy(const void *src, void *dst, s32 len);
void func_8008AE40(u8 *companyCode, u8 *gameCode) {
    _bcopy(companyCode, &D_800AA010, 2);
    _bcopy(gameCode, &D_800AA014, 4);
}
