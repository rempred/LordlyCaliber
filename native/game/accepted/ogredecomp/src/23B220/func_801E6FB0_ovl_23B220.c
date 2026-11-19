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
    u8 unk00[0xD0];
    u8 bD0;
} Ent23B;
extern u8 *D_801D0810;
extern u8 D_801EFCB0_ovl_23B220[];
Ent23B *func_801C8FE8(s16 a);
void func_801E6FB0_ovl_23B220(void) {
    D_801D0810 = D_801EFCB0_ovl_23B220;
}
s32 func_801E6FC4_ovl_23B220(void);
void func_801E6FCC_ovl_23B220(void);
void func_801E6FD4_ovl_23B220(void);
void func_801E6FDC_ovl_23B220(s16 a);
u8 func_801E7164_ovl_23B220(u8 *p);
