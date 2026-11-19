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
               s32 key;
               u8 unk04[0x40];
               void *data;
               s32 unk48;
               s32 unk4C;
               s32 unk50;
               u8 unk54[0x4E];
               u8 bytes[6];
               u16 halves[4];
} CacheEntry;
extern u8 D_801CEF00_ovl_1F0A30;
extern CacheEntry *D_801D0728_ovl_1F0A30;
extern s32 D_801D06B0_ovl_1F0A30[8];
extern u8 D_800EB0B0[];
extern u8 D_800EB100[];
extern u8 D_800EB150[];
extern u8 D_800EB1A0[];
extern u8 D_800EB1F0[];
extern u8 D_800EB240[];
extern u8 D_800EB290[];
extern u16 D_800E810E;
typedef struct {
    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;
} BBox205;
typedef struct {
    s32 unk0;
    s32 x;
    s32 y;
    s32 w;
    s32 h;
    s32 unk14[5];
} Item205;
extern BBox205 D_801D03A0_ovl_1F0A30;
extern u8 D_801CEEE0_ovl_1F0A30[];
void func_800EA6B0(void *ch, s32 a, s32 b);
void func_800EA604(void *ch, s32 sound);
void *HMalloc(s32 size);
void HFree(void *p);
void bzero(void *p, s32 size);
void func_801C1D90_ovl_1F0A30(void);
void func_801C1D98_ovl_1F0A30(void);
s32 func_801C2178_ovl_1F0A30(void *a, void *b, s32 i, s32 *r);
s32 func_801C1DA0_ovl_1F0A30(void *a, void *b);
s32 func_801C1E44_ovl_1F0A30(void *a, void *b);
BBox205 func_801C1EE8_ovl_1F0A30(void *a, void *b);
u32 func_801C2290_ovl_1F0A30(u32 v);
u16 func_801C22B8_ovl_1F0A30(u32 v);
void func_801C22D0_ovl_1F0A30(u8 bits);
void func_801C22E8_ovl_1F0A30(u8 bits);
u8 func_801C2304_ovl_1F0A30(void);
void func_801C2310_ovl_1F0A30(void);
void func_801C2710_ovl_1F0A30(u8 *o);
s32 func_801C2C5C_ovl_1F0A30(s32 key);
s32 func_801C2CA0_ovl_1F0A30(s32 key);
void * func_801C2CE4_ovl_1F0A30(s32 key);
s32 func_801C2D28_ovl_1F0A30(s32 key);
void func_801C2D6C_ovl_1F0A30(s32 key, void *data);
void func_801C2DDC_ovl_1F0A30(s32 key, s32 *a, s32 *b, s32 *c, s32 *d);
void func_801C2E40_ovl_1F0A30(s32 key, u8 *out);
void func_801C58F0_ovl_1F0A30(void);
void SoundRequest(u8 *ch, s32 sound);
void func_801C5A20_ovl_1F0A30(void) {
    if (D_801D06B0_ovl_1F0A30[0] >= 0) {
        func_800EA6B0(D_800EB0B0, D_801D06B0_ovl_1F0A30[0], 10);
    }
    if (D_801D06B0_ovl_1F0A30[1] >= 0) {
        func_800EA6B0(D_800EB100, D_801D06B0_ovl_1F0A30[1], 10);
    }
    if (D_801D06B0_ovl_1F0A30[2] >= 0) {
        func_800EA604(D_800EB150, D_801D06B0_ovl_1F0A30[2]);
    }
    if (D_801D06B0_ovl_1F0A30[3] >= 0) {
        func_800EA604(D_800EB1A0, D_801D06B0_ovl_1F0A30[3]);
    }
    if (D_801D06B0_ovl_1F0A30[4] >= 0) {
        func_800EA604(D_800EB1F0, D_801D06B0_ovl_1F0A30[4]);
    }
    if (D_801D06B0_ovl_1F0A30[5] >= 0) {
        func_800EA604(D_800EB240, D_801D06B0_ovl_1F0A30[5]);
    }
    if (D_801D06B0_ovl_1F0A30[6] >= 0) {
        func_800EA604(D_800EB290, D_801D06B0_ovl_1F0A30[6]);
    }
}
