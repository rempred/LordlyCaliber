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
               u8 unk00;
               u8 state;
               u8 unk02[2];
               u8 *base;
               u8 *volatile pc;
               u16 depth;
               u8 unk0E[2];
               u8 *stack[4];
               u16 loopDepth;
               u16 loopCount[4];
               u8 unk2A[2];
               u16 cond;
} ScriptCtx;
typedef struct {
    u8 used;
    u8 value;
    u16 id;
} ScriptSlot;
extern ScriptSlot D_8022A1F0_ovl_1BA050[];
extern u8 *D_8022A248_ovl_1BA050;
extern s32 D_8022A24C_ovl_1BA050;
extern volatile s32 D_8022A250_ovl_1BA050;
extern s32 *D_8022A214_ovl_1BA050;
extern s32 D_8022A82C_ovl_1BA050;
extern u16 D_8022A254_ovl_1BA050;
extern u16 D_8022A256_ovl_1BA050;
extern u8 D_8022A821_ovl_1BA050;
u8 *func_80222EEC_ovl_1BA050(u16 id);
void func_80229604_ovl_1BA050(s32 arg);
extern u16 D_8022A218_ovl_1BA050[];
extern u16 *D_80229CB0_ovl_1BA050[];
extern u16 D_8022A822_ovl_1BA050;
extern u16 D_8022A830_ovl_1BA050;
extern u16 D_8022A828_ovl_1BA050;
extern u16 *D_80229CE0_ovl_1BA050[];
extern u16 *D_80229CEC_ovl_1BA050[];
extern u16 *D_80229CF8_ovl_1BA050[];
extern u16 *D_80229D04_ovl_1BA050[];
extern s8 D_800E9C0A[];
u8 *func_80226164_ovl_1BA050(u8 id);
ScriptCtx *func_802219E0_ovl_1BA050(u8 *base, s32 off, s32 idx);
u8 *func_80222EA0_ovl_1BA050(u16 id);
extern u8 *D_8022A234_ovl_1BA050;
extern u8 D_801936E0;
extern u8 D_80196AEC;
void HFree(void *p);
void bzero(void *p, s32 size);
void func_80229930_ovl_1BA050(void);
void func_80228EE4_ovl_1BA050(void);
void func_80228378_ovl_1BA050(void);
void func_80224E1C_ovl_1BA050(void);
void func_80224BE8_ovl_1BA050(void);
void func_80079618(s32 handle, s32 flags);
void func_80224268_ovl_1BA050(u8 *o, u16 a, s32 b);
void func_80224320_ovl_1BA050(u8 *o, u16 a);
s32 func_80227F68_ovl_1BA050(u16 a);
void func_80220F40_ovl_1BA050(void);
extern s32 D_8022A210_ovl_1BA050;
extern s32 D_8022A230_ovl_1BA050;
void *func_8017F334(u16 id);
void func_8017C2FC(void *o);
void func_8017C384(void *o);
void *LoadAsset(s32 file, s32 id, s32 a, s32 b);
void bzero(void *p, s32 n);
typedef struct {
               u8 *vram;
               s32 unk04;
               u8 *romStart;
               u8 *romEnd;
               u8 *bssStart;
               u8 *bssEnd;
               u8 *textStart;
               u8 *textEnd;
               u8 *dataStart;
               u8 *dataEnd;
} OvlDesc;
typedef struct {
               s32 a;
               s32 b;
               void (*init)(void);
               s32 c;
               s32 d;
               u8 *e;
               s32 f;
} EvSetup;
extern OvlDesc D_80229DBC_ovl_1BA050[];
extern EvSetup D_80229E68_ovl_1BA050[];
extern s32 D_8022A238_ovl_1BA050;
extern s32 D_8022A23C_ovl_1BA050;
extern s32 D_8022A240_ovl_1BA050;
extern s32 D_8022A244_ovl_1BA050;
void osInvalICache(void *p, s32 n);
void osInvalDCache(void *p, s32 n);
void RomRead(void *rom, void *dst, s32 size);
void func_80224C60_ovl_1BA050(void);
void func_80224D60_ovl_1BA050(void);
void func_8022995C_ovl_1BA050(void);
void func_80228E88_ovl_1BA050(s32 i);
void func_80228FE8_ovl_1BA050(s32 i);
s32 func_80229040_ovl_1BA050(s32 i);
void *func_8022905C_ovl_1BA050(s32 i, void *text, s32 x, s32 y, u8 flags);
extern inline void func_80220F64_ovl_1BA050(u16 id, u8 v) {
    s32 i;
    for (i = 0; i < 8; i++) {
        if (D_8022A1F0_ovl_1BA050[i].used == 0) {
            break;
        }
    }
    if (i >= 8) {
        for (;;) {
        }
    }
    D_8022A1F0_ovl_1BA050[i].value = v;
    D_8022A1F0_ovl_1BA050[i].id = id;
    D_8022A1F0_ovl_1BA050[i].used = 1;
}
void func_80220FD0_ovl_1BA050(u16 id);
extern inline void func_8022126C_ovl_1BA050(void) {
    if (D_8022A248_ovl_1BA050 != ((void *)0) && D_8022A24C_ovl_1BA050 != 0) {
        bzero(D_8022A248_ovl_1BA050, D_8022A24C_ovl_1BA050);
    }
    D_8022A250_ovl_1BA050 = 0;
}
u8 *func_802212AC_ovl_1BA050(s32 i);
void func_802212D4_ovl_1BA050(s32 id);
void func_80221354_ovl_1BA050(void);
void func_802213A0_ovl_1BA050(u16 bits);
void func_802213EC_ovl_1BA050(u16 bits);
void func_80221438_ovl_1BA050(u16 bits);
void func_80221484_ovl_1BA050(u16 bits);
void func_80221750_ovl_1BA050(ScriptCtx *ctx);
s32 func_802218E4_ovl_1BA050(u8 *ctx);
ScriptCtx *func_802219E0_ovl_1BA050(u8 *base, s32 off, s32 idx);
ScriptCtx *func_80221A6C_ovl_1BA050(u8 *base, s32 off);
void func_80221CB8_ovl_1BA050(void);
void func_80221D30_ovl_1BA050(void);
s32 func_8022200C_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222020_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222040_ovl_1BA050(ScriptCtx *ctx);
s32 func_802220B4_ovl_1BA050(ScriptCtx *ctx);
s32 func_8022212C_ovl_1BA050(ScriptCtx *ctx);
s32 func_802221D0_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222220_ovl_1BA050(ScriptCtx *ctx);
s32 func_8022225C_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222278_ovl_1BA050(ScriptCtx *ctx);
s32 func_802222AC_ovl_1BA050(ScriptCtx *ctx);
s32 func_802222E0_ovl_1BA050(ScriptCtx *ctx);
s32 func_8022239C_ovl_1BA050(ScriptCtx *c);
s32 func_80222418_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222468_ovl_1BA050(ScriptCtx *ctx);
s32 func_802224BC_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222564_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222588_ovl_1BA050(ScriptCtx *ctx);
s32 func_802225C4_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222664_ovl_1BA050(ScriptCtx *ctx);
s32 func_8022268C_ovl_1BA050(ScriptCtx *ctx);
s32 func_802226F8_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222728_ovl_1BA050(ScriptCtx *ctx);
s32 func_8022275C_ovl_1BA050(ScriptCtx *ctx);
s32 func_802227A0_ovl_1BA050(ScriptCtx *ctx);
s32 func_8022280C_ovl_1BA050(ScriptCtx *ctx);
s32 func_8022284C_ovl_1BA050(ScriptCtx *c);
s32 func_802228DC_ovl_1BA050(ScriptCtx *c);
s32 func_8022296C_ovl_1BA050(ScriptCtx *ctx);
s32 func_802229D4_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222A38_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222AA0_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222B34_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222BDC_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222CA0_ovl_1BA050(ScriptCtx *ctx) {
    *func_80222EA0_ovl_1BA050(*(u16 *)(ctx->pc + 2)) = 3;
    ctx->pc += 4;
    return 0;
}
extern inline void func_80222CE4_clear(void) {
    if (D_8022A248_ovl_1BA050 != ((void *)0) && D_8022A24C_ovl_1BA050 != 0) {
        bzero(D_8022A248_ovl_1BA050, D_8022A24C_ovl_1BA050);
    }
    D_8022A250_ovl_1BA050 = 0;
}
s32 func_80222CE4_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222DBC_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222E08_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222E48_ovl_1BA050(ScriptCtx *ctx);
s32 func_80222E84_ovl_1BA050(ScriptCtx *ctx);
