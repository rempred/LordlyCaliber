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
               u8 unk00[0x36];
               u16 items[4];
               u8 unk3E[2];
               u32 flags;
               u8 unk44[4];
               s32 unk48;
               s32 cls;
               u8 unk50[4];
               s32 unk54;
               s32 unk58;
               u8 unk5C[0x14];
               s32 actsMax;
} BattleUnit;
typedef struct {
    s32 (*f0)(void);
    void (*f1)(void);
    void (*f2)(void);
    void (*f3)(s16);
    void (*f4)(s16);
} BattleCallbacks;
extern BattleCallbacks *D_801D0810_ovl_1F0A30;
extern BattleCallbacks D_801CFC74_ovl_1F0A30;
extern u8 D_801CFC70_ovl_1F0A30;
extern u8 D_801976D8;
extern u16 D_80197B60;
extern u8 D_80197B62;
extern struct { u8 b[12]; } D_801976DC[];
extern u8 D_800E9C0A[];
extern u8 *D_801CE8BC_ovl_1F0A30;
typedef struct {
    u8 unk0[0xD];
    u8 items[10];
    u8 unk17[2];
} SquadRec;
extern SquadRec gSquads[];
typedef struct BattleObj20 {
    void (*fn)(struct BattleObj20 *self);
} BattleObj20;
u8 func_8016E338(u8 a, u8 cls, u8 c);
s32 func_8016F5C8(u16 id);
u8 func_801C9FA4_ovl_1F0A30(s32 idx);
extern void *D_801D0824_ovl_1F0A30;
u8 func_8016DE1C(u8 a, u8 b);
u16 func_8016FA34(u16 a, u16 b, u16 c, u16 d);
s32 func_801C8B84_ovl_1F0A30(BattleUnit *u);
BattleUnit *func_801C8FE8_ovl_1F0A30(u32 i);
void HFree(void *p);
extern u8 D_801CFC8C_ovl_1F0A30[];
void func_801C8850_ovl_1F0A30(void);
s32 func_801C8864_ovl_1F0A30(void);
void func_801C889C_ovl_1F0A30(void);
void func_801C88CC_ovl_1F0A30(void);
void func_801C88FC_ovl_1F0A30(s16 a);
void func_801C893C_ovl_1F0A30(s16 a);
u8 func_801C1AA4_ovl_1F0A30(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, u8 *g, s32 h, s32 i);
void func_801C1EE8_ovl_1F0A30(s32 *out, s32 a, u8 b);
u8 func_801BC76C_ovl_1F0A30(void);
u8 func_801BC838_ovl_1F0A30(void);
u8 func_801BC914_ovl_1F0A30(void);
u8 func_801C1C1C_ovl_1F0A30(u32 a, s32 w, s32 i, u8 *c, u8 *d1, u8 *d2);
void func_801ADCBC_ovl_1F0A30(void *u);
typedef struct {
    void *name;
    s32 index;
    u8 *buf;
    s32 size;
} Arc16_20BCE0;
extern Arc16_20BCE0 D_801D0640_ovl_1F0A30;
s32 ChunkSize(s32 id);
void *LoadChunkRaw(void *dst, s32 id);
s32 ReadU32BE(void *p);
void DecodeLZ(void *dst, void *src);
void func_8007ACB0(void *arc, void *reqs, s32 n);
void *func_80071C04(s32 size);
u8 *func_801CBE18_ovl_1F0A30(s32 a, s32 x, s32 y, s32 z);
void func_801B3CB8_ovl_1F0A30();
void func_801B5524_ovl_1F0A30();
extern s32 D_801CE8FC_ovl_1F0A30;
s32 func_801C8AA4_ovl_1F0A30(s32 a);
s32 func_801C8AC0_ovl_1F0A30(s32 a);
u8 func_801C8ADC_ovl_1F0A30(void);
u8 func_801C8AEC_ovl_1F0A30(void);
u16 func_801C8AFC_ovl_1F0A30(void);
u8 func_801C8B08_ovl_1F0A30(void);
u8 func_801C8B14_ovl_1F0A30(BattleUnit *u);
s32 func_801C8B54_ovl_1F0A30(void);
extern inline s32 func_801C8B68_ovl_1F0A30(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return (u->flags >> 8) & 1;
}
extern inline s32 func_801C8B84_ovl_1F0A30(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return ((u->flags >> 8) ^ 1) & 1;
}
extern inline s32 func_801C8BA4_ovl_1F0A30(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return (u->flags >> 10) & 1;
}
s32 func_801C8BC0_ovl_1F0A30(u8 a, u8 b);
extern inline s32 func_801C8C3C_ovl_1F0A30(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return (u->flags >> 12) & 1;
}
extern inline s32 func_801C8C58_ovl_1F0A30(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return (u->flags >> 9) & 1;
}
extern inline s32 func_801C8C74_ovl_1F0A30(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return u->cls == 1;
}
s32 func_801C8C90_ovl_1F0A30(BattleUnit *u);
s32 func_801C8CD4_ovl_1F0A30(BattleUnit *u);
s32 func_801C8CF0_ovl_1F0A30(s32 c);
s32 func_801C8D24_ovl_1F0A30(BattleUnit *u);
s32 func_801C8D68_ovl_1F0A30(BattleUnit *u);
s32 IsDragon(BattleUnit *u);
s32 func_801C8DBC_ovl_1F0A30(BattleUnit *u);
s32 func_801C8DF4_ovl_1F0A30(BattleUnit *u);
extern inline s32 func_801C8E30_ovl_1F0A30(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 1;
    }
    return u->flags & 1;
}
s32 func_801C8E48_ovl_1F0A30(BattleUnit *u);
s32 func_801C8E64_ovl_1F0A30(BattleUnit *u);
s32 func_801C8E80_ovl_1F0A30(BattleUnit *u);
extern inline s32 func_801C8E9C_ovl_1F0A30(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return (u->flags >> 1) & 1;
}
s32 func_801C8EB8_ovl_1F0A30(BattleUnit *u);
s32 func_801C8ED4_ovl_1F0A30(s32 a);
s32 func_801C8F18_ovl_1F0A30(s32 a);
s32 func_801C8F24_ovl_1F0A30(BattleUnit *u);
u16 func_801C8FB8_ovl_1F0A30(BattleUnit *u);
BattleUnit *func_801C8FE8_ovl_1F0A30(u32 i);
extern inline BattleUnit *BattleGetUnit(u32 i) {
    u8 *base = D_801CE8BC_ovl_1F0A30;
    s32 off;
    if (i < 20) {
        off = i * 0xF8;
        if (*(s32 *)(base + off + 0x20C) != 0) {
            return (BattleUnit *)(base + (off + 0x1C4));
        }
    }
    return ((void *)0);
}
void func_801C9234_ovl_1F0A30(u8 *t, s32 *prev, s32 *last);
BattleUnit *func_801C92F8_ovl_1F0A30(void);
BattleUnit *func_801C9380_ovl_1F0A30(void);
BattleUnit *func_801C940C_ovl_1F0A30(void);
typedef struct Fx20B {
               void (*fn)();
               u8 unk04[0x10];
               u8 b14;
               u8 unk15[0x1F];
               s16 h34;
               u8 unk36[0xA];
               u8 *owner;
               s32 w44;
               u8 unk48[0x48];
               struct Fx20B *head;
} Fx20B;
void func_801C95D8_ovl_1F0A30(u8 *o, s32 k, s32 dx, s32 dy, s32 dz) {
    Fx20B *a;
    Fx20B *b;
    a = (Fx20B *)func_801CBE18_ovl_1F0A30((*(s32 *)((u8 *)(o) + (0x50))), (*(s32 *)((u8 *)(o) + (0x5C))) + dx, (*(s32 *)((u8 *)(o) + (0x60))) + dy,
                                          (*(s32 *)((u8 *)(o) + (0x64))) + dz);
    a->b14 = 0;
    ((Fx20B **)o)[k] = a;
    b = (Fx20B *)func_801CBE18_ovl_1F0A30((*(s32 *)((u8 *)(o) + (0x50))), (*(s32 *)((u8 *)(o) + (0x5C))) + dx, (*(s32 *)((u8 *)(o) + (0x60))) + dy,
                                          (*(s32 *)((u8 *)(o) + (0x64))) + dz);
    b->b14 = 0;
    ((Fx20B **)o)[k + 3] = b;
    b->owner = o;
    a->fn = func_801B3CB8_ovl_1F0A30;
    a->owner = o;
    b->fn = func_801B5524_ovl_1F0A30;
    a->h34 = 0xFF;
    b->h34 = D_801CE8FC_ovl_1F0A30;
    a->w44 = b->w44 = (*(s32 *)((u8 *)(o) + (0x50)));
    b->head = a;
    a->head = a;
}
void func_801C96D4_ovl_1F0A30(BattleUnit *u, BattleUnit *t);
u8 func_801C9FA4_ovl_1F0A30(s32 idx);
s32 func_801C9FB4_ovl_1F0A30(s32 a, s32 b);
s32 func_801CA000_ovl_1F0A30(s32 a, s32 b);
s32 func_801CA100_ovl_1F0A30(BattleUnit *u);
s32 func_801CA1C4_ovl_1F0A30(u32 lane, s32 row);
s32 func_801CA268_ovl_1F0A30(BattleUnit *a, BattleUnit *b);
s32 func_801CA29C_ovl_1F0A30(BattleUnit *u);
BattleUnit *func_801CA2E0_ovl_1F0A30(u32 lane, s32 row);
s32 func_801CA384_ovl_1F0A30(BattleUnit *u);
s32 func_801CA534_ovl_1F0A30(u8 *p);
void func_801CBD48_ovl_1F0A30(void);
extern inline u8 *BattleGetUnit2(u32 i) {
    u8 *base;
    s32 off;
    if (i < 20) {
        base = D_801CE8BC_ovl_1F0A30;
        off = i * 0xF8;
        if (*(s32 *)(base + off + 0x20C) != 0) {
            return base + (off + 0x1C4);
        }
    }
    return ((void *)0);
}
typedef struct {
    s32 w[20];
} Pose20B;
extern inline void bumpAnim20B(u8 *e) {
    if (e != ((void *)0) && e[0x36] < e[0x37]) {
        e[0x36]++;
    }
}
void func_801CBF44_ovl_1F0A30(void);
void *func_801CC124_ovl_1F0A30(s32 id);
s32 func_801CC37C_ovl_1F0A30(void);
s32 func_801CC44C_ovl_1F0A30(void);
s32 func_801CC51C_ovl_1F0A30(s32 id);
