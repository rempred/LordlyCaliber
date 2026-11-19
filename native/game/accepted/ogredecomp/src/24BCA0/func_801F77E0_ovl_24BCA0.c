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
void HFree(void *p);
s32 func_801C8AC0(s32 a);
s32 func_801C8AA4(s32 a);
s32 func_801CA1C4(s32 lane, s32 row);
u8 *func_801C8FE8(s32 i);
void func_801F7344_ovl_24BCA0(u8 a, u8 *out, u8 b);
void func_801F85C0_ovl_24BCA0(u8 *a, u8 *b);
void func_801F885C_ovl_24BCA0(u8 *a, u8 *b);
void func_801F8AF8_ovl_24BCA0(u8 *a, u8 *b, f32 c);
void func_801F91BC_ovl_24BCA0(u8 *a, u8 *b, s32 c);
u8 *func_802174C0(u16 id);
void func_8020A4D0(f32 *scale, void *a, void *b);
u8 func_801F70E0_ovl_24BCA0(u8 c);
typedef struct {
    u16 id;
    u8 kind;
    u8 pad;
    u16 flags;
} AnimReq;
extern u8 *(*D_80209718_ovl_24BCA0[])(u16 id);
extern u8 *(*D_80209728_ovl_24BCA0[])(u16 id);
extern u8 *(*D_80209734_ovl_24BCA0[])(u16 id);
extern u8 *(*D_80209740_ovl_24BCA0[])(u16 id, u8 x);
extern u8 *(*D_8020974C_ovl_24BCA0[])(u16 id, s32 a, s32 x, s32 b, s32 y);
extern s32 D_8020A170_ovl_24BCA0[];
u8 *func_80214CF0(u16 id, s32 a);
extern s32 (*D_80209700_ovl_24BCA0[])(u8 c);
extern u32 D_8020A2D4_ovl_24BCA0;
extern u8 *D_80209770_ovl_24BCA0[];
void func_80218104(f32 x, f32 y, s32 z, s32 w);
s32 func_801C8E30(u8 *u);
extern u16 D_801CEF10[];
void func_801C5938(void *a, u16 b);
s32 guRandom(void);
void func_801BC3A0(s16 x, s16 y, s32 a, s32 b);
void func_8020A348(f32 *a, f32 *b, f32 *out);
void guMtxIdentF(f32 (*m)[4]);
s32 func_801F71A0_ovl_24BCA0(u8 c);
s32 func_801F71CC_ovl_24BCA0(u8 c);
s32 func_801F71FC_ovl_24BCA0(f32 x);
s32 func_801F722C_ovl_24BCA0(f32 x);
s32 func_801F725C_ovl_24BCA0(u8 c);
s32 func_801F729C_ovl_24BCA0(u8 c);
s32 func_801F72E0_ovl_24BCA0(s32 a);
s32 func_801F730C_ovl_24BCA0(s32 a);
void func_801F7644_ovl_24BCA0(u8 *str, f32 *out, u8 c);
s16 func_801F7738_ovl_24BCA0(f32 *pos);
s16 func_801F77E0_ovl_24BCA0(u8 c) {
    s32 v = c - 20;
    return func_801CA1C4(v % 3, v / 3);
}
s16 func_801F782C_ovl_24BCA0(u8 i);
void func_801F7BE4_ovl_24BCA0(f32 *d, f32 *a, f32 *b);
void func_801F7C18_ovl_24BCA0(f32 *dir, f32 (*m)[4]);
void func_801F8180_ovl_24BCA0(u8 *p);
extern inline void func_801F81C4_ovl_24BCA0(u8 *p, u16 f) {
    s16 v;
    (*(u16 *)((u8 *)(p) + (0xA))) |= f & 0xD000;
    if (f & 0x1000) {
        if (f & 0x400) {
            v = (f & 0x800) ? 10000 : -10000;
        } else {
            v = (f & 0x800) ? 1000 : -1000;
        }
        (*(s32 *)((u8 *)(p) + (0xC))) = v;
    }
}
u8 *func_801F8218_ovl_24BCA0(AnimReq *r);
u8 *func_801F82B4_ovl_24BCA0(AnimReq *r);
u8 *func_801F8350_ovl_24BCA0(AnimReq *r);
u8 *func_801F83EC_ovl_24BCA0(AnimReq *r, s32 i);
u8 *func_801F847C_ovl_24BCA0(AnimReq *r, u8 x);
u8 *func_801F8518_ovl_24BCA0(AnimReq *r, s32 x);
void func_801F8E50_ovl_24BCA0(u8 a, u8 *b);
void func_801F8E90_ovl_24BCA0(u8 a, u8 *b);
void func_801F8ED0_ovl_24BCA0(u8 a, u8 *b, f32 c);
void func_801F93BC_ovl_24BCA0(u8 a, u8 *b, s32 c);
void func_801F9848_ovl_24BCA0(u16 id);
u8 *func_801F9870_ovl_24BCA0(u16 id, f32 scale);
void func_801F98D0_ovl_24BCA0(u8 a, u8 *b, u8 n);
void func_801F9CD8_ovl_24BCA0(u8 a, s32 b);
extern inline s32 cls24(u8 c) {
    s32 k;
    if (c < 20) {
        k = 0;
    } else if ((u8)(c - 20) < 27) {
        k = 1;
    } else if ((u8)(c - 47) < 40) {
        k = 2;
    } else {
        k = 0xFF;
    }
    return D_80209700_ovl_24BCA0[k](c) >= 0;
}
