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
extern void *D_80213EE8_ovl_14EBE0;
void *HFree(void *p);
extern u8 D_801F36A0;
extern s32 D_801F401C;
extern u8 D_80213EA4_ovl_14EBE0[];
void *LoadChunk(s32 id);
void func_8020CE00_ovl_14EBE0(void);
void func_8020CE0C_ovl_14EBE0(void) {
    u8 f;
    if (D_80213EE8_ovl_14EBE0 == ((void *)0)) {
        D_80213EE8_ovl_14EBE0 = LoadChunk(0x1DC326E);
    }
    f = D_801F36A0;
    if ((f & 1) && !(f & 4) && D_801F401C < 100) {
        s32 i;
        if (!(f & 2)) {
            i = 0x30;
        } else {
            i = 0x18;
        }
        D_80213EA4_ovl_14EBE0[i] = 1;
    }
}
void func_8020CE8C_ovl_14EBE0(void);
