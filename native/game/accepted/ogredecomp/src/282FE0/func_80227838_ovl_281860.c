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
extern u8 D_800EB0B0[];
extern s32 D_8022A8A8_ovl_281860[];
typedef struct {
    s16 h0;
    s16 h2;
    s16 h4;
    s16 h6;
} St8C4;
extern St8C4 *D_8022A8C4_ovl_281860;
extern s32 D_8022A990_ovl_281860;
extern u8 D_8018FC19;
extern u8 D_8022A980_ovl_281860;
typedef struct {
    s16 h0;
    s16 h2;
    u8 b4;
    u8 b5;
    u8 b6;
    u8 b7;
    u8 b8;
    u8 b9;
    u8 bA;
    u8 bB;
} StFC10;
extern StFC10 *D_8018FC10;
extern u8 *D_8022A974_ovl_281860;
void func_800EA838(void *p);
void func_800EA884(s32 a);
void HFree(void *p);
void MusSetMasterVolume(s32 flags, s32 volume);
typedef struct {
    u8 b[0x34];
} EChar282;
typedef struct {
    u8 b[0x38];
} PChar282;
typedef struct Node282 {
    struct Node282 *prev;
    struct Node282 *next;
} Node282;
extern EChar282 D_80195560[];
extern PChar282 D_80193BC0[];
extern void *D_8018EA00[];
extern Node282 *D_8018FDC0;
extern s32 D_8022A720_ovl_281860;
extern s32 D_8022A734_ovl_281860;
extern s32 D_8022A738_ovl_281860;
extern s32 D_8022A73C_ovl_281860;
extern s32 D_8022A740_ovl_281860;
extern s32 D_8022A744_ovl_281860;
extern s32 D_8022A748_ovl_281860;
extern s32 D_8022A778_ovl_281860;
extern s32 D_8022A78C_ovl_281860;
extern s32 D_8022A790_ovl_281860;
extern s32 D_8022A794_ovl_281860;
extern s32 D_8022A798_ovl_281860;
extern s32 D_8022A79C_ovl_281860;
extern s32 D_8022A7A0_ovl_281860;
s32 func_8016FEF4(s32 flag);
void func_800EA7E4(s32 a, s32 b);
void func_802282B8_ovl_281860(void);
void func_800EA86C(s32 ch, s32 v);
void func_800EA89C(s32 ch, s32 v);
void func_800EA8B4(s32 ch, s32 v);
extern s32 D_800EB2E0[];
void *HMalloc(s32 size);
void bzero(void *p, s32 n);
void func_8022681C_ovl_281860(s32 a, s32 b);
extern u8 D_801976DC;
extern u8 D_801971F2[];
extern u8 D_8022AC40_ovl_281860[];
extern u8 D_8022A921_ovl_281860[];
extern u8 *D_8022A8C8_ovl_281860[];
void func_8009C550(u8 *dst, u8 *src);
void func_8009C620(u8 *dst, u8 *src);
void func_80227010_ovl_281860(void);
void func_80227210_ovl_281860(s32 i);
void func_80227254_ovl_281860(s32 mode, s32 idx, s32 n, s32 h);
void func_80227348_ovl_281860(void);
void func_802274F4_ovl_281860(s32 i, s32 v);
void func_80227594_ovl_281860(s32 i, s32 a, s32 b, s32 c);
void func_8022767C_ovl_281860(void);
void func_80227684_ovl_281860(void);
void func_80227694_ovl_281860(void);
s32 func_802276C4_ovl_281860(void);
void func_802276D4_ovl_281860(void);
void func_802276E0_ovl_281860(u8 v);
u8 func_802276EC_ovl_281860(void);
s16 func_802276F8_ovl_281860(s32 i);
void func_80227770_ovl_281860(void);
void func_80227778_ovl_281860(s32 i);
void *func_80227838_ovl_281860(s32 id) {
    s32 i;
    for (i = 0; i < 100; i++) {
        if (D_80195560[i].b[0x11] == id) {
            return &D_80195560[i];
        }
    }
    for (i = 0; i < 100; i++) {
        if (D_80193BC0[i].b[0x11] == id) {
            return &D_80193BC0[i];
        }
    }
    if (id >= 0x54) {
        return D_8018EA00[id];
    }
    return ((void *)0);
}
void func_802278DC_ovl_281860(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
void func_802279D8_ovl_281860(void);
s16 func_802279F4_ovl_281860(void);
void func_80227B60_ovl_281860(void);
void func_80227B80_ovl_281860(void);
s32 func_80227DC4_ovl_281860(s32 a, s32 b, s32 c, s32 d);
