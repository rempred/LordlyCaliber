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
extern u8 D_80239140_ovl_29A4C0[];
extern u8 D_8023914C_ovl_29A4C0[];
void func_8020DFB8(void);
void func_8020DC80(u16 state);
void func_8020E0B4(s32 a, s32 b);
void func_8020B538(s32 a, u8 *s);
s32 func_8023823C_ovl_29A4C0(u16 msg) {
    switch (msg) {
        case 0x101:
            func_8020DFB8();
            break;
        case 0x100:
            func_8020DC80(0x1010);
            break;
        case 0x1010:
            func_8020E0B4(10, 0x1100);
            break;
        case 0x1100:
            func_8020B538(4, D_80239140_ovl_29A4C0);
            func_8020E0B4(10, 0x1200);
            break;
        case 0x1200:
            func_8020B538(4, D_8023914C_ovl_29A4C0);
            func_8020E0B4(10, 0x101);
            break;
    }
    return 0;
}
