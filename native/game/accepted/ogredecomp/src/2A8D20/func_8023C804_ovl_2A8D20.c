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
extern u8 *D_8022A974;
extern u8 *D_8022A978;
extern u8 D_8022A980;
void HFree(void *p);
void func_80227B80(void);
void func_8023BDF8_ovl_2A8D20();
extern u8 *D_801CE8BC;
u8 func_8016FF5C(u8 a, u8 b);
void *HMalloc(s32 size);
s32 func_801C8B68(u8 *u);
s32 func_801C8C90(u8 *u);
u8 func_801C1AA4(s32 a, s32 b, u8 c, s16 d, s16 e, s32 f, u8 *g, u8 *h, u8 *i);
extern s16 D_8022A994;
extern u8 D_8018FC19;
extern u8 D_801976E8;
void bzero(void *p, s32 n);
void func_801C22D0(s32 a);
void func_801C45E0(void);
void func_8020B098(void);
void func_801CBD48(void);
void func_801BD1F4(void);
void func_801707F4(void);
void func_80227FD8(s16 a, s32 b);
void func_8022D120(void);
void func_80227010(void);
void func_8023998C_ovl_2A8D20(void);
u8 *func_801C8FE8(s32 i);
void func_801C4778(s32 a);
void func_801ADB00(void *p);
void func_801ADBC0(void *p);
void func_8023D2DC_ovl_2A8D20(s32 a);
extern u8 D_8022A980;
extern f32 D_801CE8CC;
extern f32 D_801CE8D0;
extern f32 D_801CE8D4;
extern f32 D_801CE8D8;
extern f32 D_801CE8DC;
extern f32 D_801CE8E0;
extern f32 D_801CE8E8;
extern f32 D_8022A720;
extern f32 D_8022A734;
extern f32 D_8022A738;
extern f32 D_8022A73C;
extern f32 D_8022A740;
extern f32 D_8022A744;
extern f32 D_8022A748;
void func_802395C0_ovl_2A8D20(void);
s32 func_8023965C_ovl_2A8D20(u8 *o);
extern inline void func_8023977C_ovl_2A8D20(u8 a, u8 b) {
    u8 *ctx = D_8022A974;
    u8 *pa = (*(u8 * *)((u8 *)(ctx + a * 4) + (0x18)));
    u8 *pb = (*(u8 * *)((u8 *)(ctx + b * 4) + (0x18)));
    (*(u8 * *)((u8 *)(ctx + a * 4) + (0x18))) = pb;
    (*(u8 * *)((u8 *)(ctx + b * 4) + (0x18))) = pa;
    if (pa != ((void *)0)) {
        (*(s32 *)((u8 *)(pa) + (0xE4))) = b;
    }
    if (pb != ((void *)0)) {
        (*(s32 *)((u8 *)(pb) + (0xE4))) = a;
    }
}
void func_8023998C_ovl_2A8D20(void);
void func_80239A84_ovl_2A8D20(f32 x, f32 y, f32 z, f32 w, s32 n);
void func_80239AF8_ovl_2A8D20(void);
s32 func_80239B68_ovl_2A8D20(void);
u8 func_80239B88_ovl_2A8D20(void);
u8 func_8023A0FC_ovl_2A8D20(s32 i);
s32 func_8023AEA8_ovl_2A8D20(u8 *p);
s16 func_8023B428_ovl_2A8D20(s32 id, s32 mode);
u8 *func_8023B494_ovl_2A8D20(void);
s32 func_8023BD7C_ovl_2A8D20(void);
void func_8023BD9C_ovl_2A8D20(u8 v);
void func_8023BDA8_ovl_2A8D20(s32 f);
void func_8023BDDC_ovl_2A8D20(void);
s32 func_8023C804_ovl_2A8D20(s32 i) {
    u8 *o = (*(u8 * *)((u8 *)(D_8022A974 + (i) * 4) + (0x18)));
    s32 r;
    if (o == ((void *)0) || o[0x147] == 0xFF) {
        return 0;
    }
    r = func_801C8B68(D_801CE8BC + 0x1C4 + o[0x147] * 0xF8);
    if (r == 0) {
        return 1;
    }
    return 2;
}
void func_8023C9BC_ovl_2A8D20(void);
typedef struct {
    u8 unk00[0x48];
    s32 id;
    u8 unk4C[0xAC];
} Unit2A8;
typedef struct {
    u8 unk000[0x1C4];
    Unit2A8 units[20];
} Army2A8;
void func_8023D0EC_ovl_2A8D20(u8 slot, u8 kind);
void func_8023D7A8_ovl_2A8D20(void);
s32 func_8023DCC0_ovl_2A8D20(void);
void func_8023DF70_ovl_2A8D20(s32 w, s32 h, s32 x, s32 y, s32 a, s32 b);
s32 func_8023E2C0_ovl_2A8D20(void);
void func_8023E2E8_ovl_2A8D20(void);
