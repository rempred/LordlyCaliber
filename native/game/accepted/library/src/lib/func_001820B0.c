typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u16 flags;
    u8 flags2;
    u8 flags3;
    u8 unk04[2];
    s16 x;
    s16 y;
    s16 x2;
    s16 y2;
    u16 unk0E;
    void *owner;
    u8 unk14[0x14];
    s16 unk28;
    s16 unk2A;
    u8 unk2C[0xC];
    void *slotsA[10];
    void *slotsB[10];
    u8 unk88;
    u8 unk89;
    u8 unk8A;
    u8 unk8B;
    u8 unk8C;
    u8 unk8D;
    u8 unk8E;
    u8 unk8F;
    u8 unk90[4];
    u8 unk94;
    u8 unk95;
    u8 unk96;
    u8 unk97;
    u8 unk98[0xA];
    s16 parent;
    u8 unkA4[4];
} Scene;
extern Scene g_state_slot_records[];
typedef struct {
    s32 data[0x16];
} Camera;
typedef struct {
    Camera *saved;
    s32 arg;
    s16 timer;
    s16 alpha;
    s16 *alphaPtr;
    s16 unk10;
    s32 unk14;
} Ovl17F9C0Ctx;
extern Ovl17F9C0Ctx *D_8021AF00;
extern u8 D_8021AF04;
extern Camera *D_8018F58C;
extern u8 g_func_0015DF10_default[];
extern u8 D_801F365D;
extern u8 D_800E7A90[];
extern s32 D_801F3620;
void *func_00001330(s32 size);
void *func_000016C4(void *p);
void D_800EA604(void *ch, s32 sound);
s32 func_00181520(void);
void D_801DFF50(void);
void D_801DFF80(void);
void D_8017BED4(void);
extern inline void func_80217474_ovl_17F9C0(s32 arg) {
    D_8021AF00 = func_00001330(sizeof(Ovl17F9C0Ctx));
    D_8021AF00->arg = arg;
    D_8021AF00->saved = func_00001330(sizeof(Camera));
    *D_8021AF00->saved = *D_8018F58C;
    D_8021AF00->timer = 0;
    D_8021AF00->alpha = 0xFF;
}
extern inline void func_8021751C_ovl_17F9C0(void) {
    *D_8018F58C = *D_8021AF00->saved;
    func_000016C4(D_8021AF00->saved);
    func_000016C4(D_8021AF00);
    D_8021AF00 = ((void *)0);
    D_8021AF04 = 0;
}
extern inline void func_802175B0_ovl_17F9C0(void) {
    Ovl17F9C0Ctx *ctx = D_8021AF00;
    if (ctx->timer == 0) {
        D_800EA604(g_func_0015DF10_default, 0x288);
    }
    D_8021AF00->timer++;
    ctx->alphaPtr = &D_8021AF00->alpha;
    ctx->unk10 = 0;
    ctx->unk14 = 0x26;
    func_00181520();
    if (D_8021AF00->timer < 50) {
        D_8021AF04 = 4;
    } else {
        D_8021AF04 = 1;
    }
}
s32 func_001820B0(void) {
    s32 ret = 0;
    D_801F365D |= 1;
    g_state_slot_records[D_800E7A90[0x29]].unk8B |= 4;
    if (D_8021AF04 == 0) {
        D_801DFF50();
        D_8017BED4();
        func_80217474_ovl_17F9C0(D_801F3620);
    }
    func_802175B0_ovl_17F9C0();
    if (D_8021AF04 == 1) {
        func_8021751C_ovl_17F9C0();
        ret = 1;
        D_801DFF80();
    }
    return ret;
}
