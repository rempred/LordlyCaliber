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
              u8 unk0[4];
              u16 w;
              u16 h;
} Size1F;
extern s32 D_801CEAB4_ovl_1F0A30;
extern s32 D_801D079C_ovl_1F0A30;
extern s32 D_801D07CC_ovl_1F0A30;
extern s32 D_801D07E4_ovl_1F0A30;
extern s32 D_801D07E8_ovl_1F0A30;
extern s32 D_801D07EC_ovl_1F0A30;
extern s16 D_801D07F4_ovl_1F0A30;
extern s16 D_801D07F6_ovl_1F0A30;
extern s16 D_801D07F8_ovl_1F0A30;
extern s16 D_801D07FA_ovl_1F0A30;
extern s16 D_801D07FC_ovl_1F0A30;
extern s16 D_801D07FE_ovl_1F0A30;
extern s16 D_801D0800_ovl_1F0A30;
extern s16 D_801D0802_ovl_1F0A30;
extern s16 D_801D0804_ovl_1F0A30;
extern s16 D_801D0806_ovl_1F0A30;
extern s16 D_801D0808_ovl_1F0A30;
extern s16 D_801D080A_ovl_1F0A30;
extern u8 D_801D080C_ovl_1F0A30;
extern u8 D_801D080D_ovl_1F0A30;
extern s32 D_801D0770_ovl_1F0A30[];
extern void *D_801D0748_ovl_1F0A30[];
extern void *D_801D074C_ovl_1F0A30;
extern void *D_801D0750_ovl_1F0A30;
extern void *D_801D0754_ovl_1F0A30;
extern u8 D_801D0828_ovl_1F0A30[];
extern u8 *D_801CE8C0_ovl_1F0A30;
extern u8 D_801976DC;
extern u8 D_801976E8;
void bzero(void *p, s32 n);
void *HFree(void *p);
void func_801B594C_ovl_1F0A30(Size1F *a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
void func_801BC15C_ovl_1F0A30(s16 a, s16 b, s32 c, s32 d, s32 e);
void func_801C5D64_ovl_1F0A30(void);
void func_801BDC78_ovl_1F0A30(void);
void func_801C62E4_ovl_1F0A30(s32 a, u8 *b, u8 c);
extern u8 D_80197178[];
extern void *D_800E9BA0;
extern u8 D_801969B0[];
void func_801B5D58_ovl_1F0A30(void *a, void *b, u8 *img);
void func_801B619C_ovl_1F0A30(void *a, void *b, u8 *img, u8 c, s32 d, s32 e);
void func_801B6970_ovl_1F0A30(void *a, void *b, u8 *img, u8 c, s32 d, s32 e);
void func_801B6DB0_ovl_1F0A30(void *a, void *b, u8 *img);
void func_801B7284_ovl_1F0A30(u8 *img, s32 x, s32 y, s32 s, void *b, void *m, void *a);
void func_801B58E0_ovl_1F0A30(void);
s32 func_801B58F8_ovl_1F0A30(void);
void func_801B5904_ovl_1F0A30(void);
void func_801B591C_ovl_1F0A30(void);
s32 func_801B5934_ovl_1F0A30(void);
s32 func_801B5940_ovl_1F0A30(void);
void func_801B5CAC_ovl_1F0A30(Size1F *s, s32 a, s32 b);
void func_801B5CE0_ovl_1F0A30(u16 *p, s16 a, s16 b, s16 c, s16 d);
void func_801B77CC_ovl_1F0A30(void *a, void *b, u8 *img, u8 c, s32 d, s32 e);
void func_801B7CBC_ovl_1F0A30(u8 *p) {
    if (p != ((void *)0)) {
        if (p[0] == 'B' && p[1] == '5') {
            u8 *bank = p;
            u8 *e = *(u8 **)(p + 0x14);
            u32 i;
            for (i = 1; i < bank[3]; i++) {
                u8 *cur = e;
                e = *(u8 **)(cur + 0xC);
                HFree(cur);
            }
        }
        HFree(p);
    }
}
void func_801BC35C_ovl_1F0A30(s16 a, s16 b);
void func_801BC3A0_ovl_1F0A30(s16 a, s16 b, s16 c, s16 d);
void func_801BC3D8_ovl_1F0A30(s16 a, s16 b, s16 c, s16 d, u8 e);
void func_801BC5FC_ovl_1F0A30(void);
s32 func_801BCFE4_ovl_1F0A30(void);
void func_801BD1F4_ovl_1F0A30(void);
void func_801BDF40_ovl_1F0A30(void);
