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
               u8 unk00[0x1C];
               s16 x;
               s16 h1E;
               s16 y;
} Pt21;
typedef struct {
               u8 unk00[0x40];
               s32 flags;
               u8 unk44[0x50];
               s32 w94;
} Msg21;
extern u8 D_801976E8[];
void func_801D90E0_ovl_213B10(void);
void func_80172394(void);
s32 func_8017223C(s32 a);
void func_801707F4(void);
f32 func_801BC35C(s16 a, s16 b);
void func_801D9FBC_ovl_213B10(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
void func_801D9F60_ovl_213B10(s32 a, s32 b, s32 c, s32 d);
void func_801E0D34_ovl_213B10(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g, s32 h, s32 i);
u8 *func_801E14FC_ovl_213B10(s32 enemy);
typedef struct {
    s32 w[20];
} Blob50;
s32 func_801C8B68(u8 *u);
s32 func_801C8BA4(u8 *u);
u16 func_801C8FB8(u8 *u);
void func_801C41C8(s32 *a, s32 *b, s32 *c, s32 *d, s32 *e, s32 f);
f32 func_8007FE70(f32 *a, f32 *b);
f32 func_801BE1E0(s32 cls, s32 base);
f32 func_801BE264(s32 cls, s32 base);
void func_801AD9D4(Blob50 *b, s32 a);
s32 func_801AE4EC(s32 a, Blob50 *b, s32 c);
s32 func_801DFB80_ovl_213B10(u8 *o, s32 a, u8 b);
typedef struct {
    u8 b0;
    u8 flags;
    u8 members[5];
    u8 rest[4];
} Squad222;
extern u8 D_801969B9[];
extern u8 D_801969BA[];
s32 func_801C8B08(u8 *u);
s32 func_801C8B84(u8 *u);
s32 func_801C8C58(u8 *u);
u8 func_8016FF5C(u8 a, u8 b);
s32 func_801DF224_ovl_213B10(void);
void func_801DF260_ovl_213B10(Pt21 *a, Pt21 *b);
s32 func_801DF2AC_ovl_213B10(Blob50 *o, s32 a, s32 c);
s32 func_801DF334_ovl_213B10(u8 *u, u32 f);
void func_801E05A4_ovl_213B10(u8 *o, s32 a, s32 b, s32 c, u8 d);
void func_801E063C_ovl_213B10(Msg21 *m, s32 a, s32 b, s32 c);
void func_801E06A8_ovl_213B10(Msg21 *m, s32 a, s32 b);
void func_801E0708_ovl_213B10(Msg21 *m, s32 a, s32 b);
void func_801E1014_ovl_213B10(s32 a, s32 b, s32 c, s32 d, s32 e, u8 f);
void func_801E104C_ovl_213B10(s32 a, s32 b, s32 c, s32 d, s32 e);
void func_801E145C_ovl_213B10(Msg21 *m, s32 a, s32 b, s32 d, s32 e) {
    func_801D9FBC_ovl_213B10(0x36, b, (s32)m, d, e >> 8, e & 0xFF);
    m->w94 = b + a;
}
extern inline u8 *func_801E14C0_ovl_213B10(s32 side) {
    u8 *p = D_801976E8;
    if (!side) {
        p -= 12;
    }
    if (p[1] & 1) {
        return func_801E14FC_ovl_213B10(side);
    }
    return ((void *)0);
}
u8 func_801E1590_ovl_213B10(s32 side);
void func_801E15E0_ovl_213B10(u8 *u, s32 cls);
