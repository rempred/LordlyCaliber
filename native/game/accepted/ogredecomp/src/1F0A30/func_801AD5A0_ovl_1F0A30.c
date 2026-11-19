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
               u8 unk00[4];
               s16 h4;
               s16 h6;
               s16 h8;
               u8 unk0A[2];
               s16 hC;
               s16 hE;
               s16 h10;
               u8 marks[0x10];
               u8 zero[0x10];
               u8 unk32[0x14];
               u8 b46;
               u8 b47;
} Panel1F;
typedef struct {
                 u8 unk0000[0x1C0];
                 s32 strategy;
                 u8 unk01C4[0x5020];
                 s32 listB[0x36];
                 s32 nB;
                 s32 listA[0x100];
                 s32 nA;
} Ctx1F;
typedef struct {
               u8 unk00[0x1C];
               s16 x;
               u8 unk1E[2];
               s16 y;
} Spr1F;
typedef struct {
               u8 unk00[8];
               f32 x;
               f32 y;
               f32 vx;
               f32 vy;
               f32 ax;
               f32 ay;
} Mot1F;
typedef struct {
              u8 unk0[4];
              s16 flag;
} Opt1F;
typedef struct {
              void *data;
              s32 ttl;
              s32 id;
} Slot1F;
typedef struct {
    u8 unk00[0x17];
    u8 strategy;
    u8 unk18;
} SquadRec;
extern u8 D_800EB1F0[];
extern Ctx1F *D_801CE8BC_ovl_1F0A30;
extern s32 *D_801CE8C0_ovl_1F0A30;
extern u8 D_801976DC;
extern SquadRec gSquads[];
extern Opt1F D_801CE900_ovl_1F0A30[];
extern u8 D_801939CC;
extern u8 D_801CE8A8_ovl_1F0A30[];
void SoundRequest(void *ch, s32 id);
void *memset(void *p, s32 c, s32 n);
void bzero(void *p, s32 n);
void func_801AEA90_ovl_1F0A30();
void func_801CA680_ovl_1F0A30(s32 a);
s32 func_8016FEF4(s32 flag);
void func_801AF730_ovl_1F0A30(void *a, s32 b);
void HFree(void *p);
u16 func_801794C8(s32 a);
void func_8017C4B4(s16 a);
void func_80179034(s32 a, s32 x, s32 y);
void *func_801AD60C_ovl_1F0A30(s32 id);
void func_801B0770_ovl_1F0A30(u8 *o, s32 a, s32 b, s32 c, s32 d, s32 e);
extern s16 D_801D071C_ovl_1F0A30;
extern s16 D_801D0820_ovl_1F0A30;
extern f32 D_801D06F0_ovl_1F0A30;
extern f32 D_801D0718_ovl_1F0A30;
extern f32 D_801D0740_ovl_1F0A30;
extern f32 D_801D0764_ovl_1F0A30;
extern f32 D_801D0768_ovl_1F0A30;
extern f32 D_801D0814_ovl_1F0A30;
extern s32 D_801D0838_ovl_1F0A30;
extern s32 D_801D0734_ovl_1F0A30;
extern s32 D_801D070C_ovl_1F0A30;
extern void *D_801CEAB0_ovl_1F0A30;
s32 func_801BE6BC_ovl_1F0A30(void *p);
s32 func_801BE6DC_ovl_1F0A30(void *p);
s32 func_801BE6FC_ovl_1F0A30(void *p);
void func_801B464C_ovl_1F0A30(s32 a, f32 x, f32 y, f32 z, s32 b);
void *LoadChunk(s32 id);
typedef struct {
    u8 b[0x38];
} PChar1F;
typedef struct {
    u8 b[0x34];
} EChar1F;
extern PChar1F D_80193BC0[];
extern EChar1F D_80195560[];
extern u8 D_801976E8;
extern signed char D_801CE890_ovl_1F0A30[];
s32 func_801C8B68_ovl_1F0A30(u8 *u);
s32 func_801C8BA4_ovl_1F0A30(u8 *u);
s32 func_801C8B84_ovl_1F0A30(u8 *u);
void BattleAnimStep(void *a, s32 b, s32 c, s32 d, s32 e);
s32 func_8016E1DC(u8 a, u8 b);
s32 sprintf(signed char *buf, const signed char *fmt, ...);
void func_801AF9B8_ovl_1F0A30(signed char *s, s32 side);
void func_801AD5A0_ovl_1F0A30(void) {
    SoundRequest(D_800EB1F0, 0);
}
void func_801AD5C4_ovl_1F0A30(void);
void func_801AD5E8_ovl_1F0A30(void);
void *func_801AD60C_ovl_1F0A30(s32 id);
void func_801AD974_ovl_1F0A30(Panel1F *p, s16 a);
s32 func_801ADACC_ovl_1F0A30(u8 *a, u8 *b);
void func_801ADADC_ovl_1F0A30(s32 v);
void func_801ADB9C_ovl_1F0A30(s32 v);
void func_801ADBC0_ovl_1F0A30(s32 v);
void func_801ADC60_ovl_1F0A30(Spr1F *s, Mot1F *m);
void func_801ADCBC_ovl_1F0A30(u8 *u);
void func_801AE8CC_ovl_1F0A30(void);
void func_801AECA4_ovl_1F0A30(s32 a);
extern inline Opt1F *func_801AED14_ovl_1F0A30(s32 i) {
    if (func_8016FEF4(D_801CE900_ovl_1F0A30[i].flag) && ((D_801939CC >> i) & 1)) {
        return &D_801CE900_ovl_1F0A30[i];
    }
    return ((void *)0);
}
s32 func_801AED84_ovl_1F0A30(void);
void func_801AFB0C_ovl_1F0A30(u8 *u, u8 *a);
void func_801AFBE8_ovl_1F0A30(s32 a);
void func_801B0284_ovl_1F0A30(u8 *o, s32 i);
s32 func_801B3C3C_ovl_1F0A30(u8 *o);
s32 func_801B5524_ovl_1F0A30(s32 a);
