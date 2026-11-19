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
extern u8 *D_801CE8BC;
extern u8 *D_8022A974;
s32 func_801C8B68(u8 *u);
s32 func_801C8C58(u8 *u);
void func_8024387C_ovl_2AE3C0(u8 *u, s32 i, s32 a, s32 id, s32 b, u8 slot, s16 c);
extern u8 *D_801CE8C0;
extern u8 D_8022A7F0;
void *HMalloc(s32 size);
void bzero(void *p, s32 n);
void func_801BE71C(void);
void func_801C1D90(void);
void func_801C1A94(void);
void func_801C2310(void);
void func_801AECA4(u8 a);
void func_80243810_ovl_2AE3C0(void);
s32 func_80243AC8_ovl_2AE3C0(s16 slot, s16 b) {
    s32 i;
    u8 *u;
    for (i = 0; i < 20; i++) {
        u = D_801CE8BC + 0x1C4 + i * 0xF8;
        if ((*(s32 *)((u8 *)(u) + (0x48))) != 0 && !func_801C8B68(u) && func_801C8C58(u)) {
            if ((*(u8 * *)((u8 *)(D_8022A974 + slot * 4) + (0x18))) != ((void *)0)) {
                func_8024387C_ovl_2AE3C0(u, i, (*(s32 *)((u8 *)(u) + (0x58))) < 4, (*(s32 *)((u8 *)(u) + (0x48))), (*(s32 *)((u8 *)(u) + (0x4C))),
                                         slot, b);
                return 0;
            }
        }
    }
    return 0;
}
