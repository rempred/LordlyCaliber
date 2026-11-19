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
extern u8 *D_801AB850;
void func_801BF2F0_ovl_101CE0(void) {
    u8 *s = D_801AB850;
    if (s[0x18] & 1) {
        if (*(s32 *)(s + 0x1C) < 4) {
            if (++*(s32 *)(s + 0x1C) == 4) {
                s[0x20] = 0;
            }
        }
    } else if (*(s32 *)(s + 0x1C) > 0) {
        (*(s32 *)(s + 0x1C))--;
    }
}
