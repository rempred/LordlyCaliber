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
               u16 flags;
               u8 pref;
               u8 unk03[0x21];
} LocationRec;
extern LocationRec D_801951CC[];
extern u8 gChaosFrame;
extern u8 D_8018F481;
extern u8 gStageTable[][12];
extern u8 D_801939CC;
extern u16 D_801F1060_ovl_101CE0[];
extern s32 D_801F0DE0_ovl_101CE0;
extern s32 D_801F3610_ovl_101CE0;
extern u8 D_801F365D_ovl_101CE0;
void bzero(void *p, s32 n);
s32 func_80186210(s32 flag);
s32 func_8016FEF4(s32 flag);
void func_801B2B78_ovl_101CE0(u32 *unit);
typedef struct {
               signed char *name;
               u8 size;
               u8 gender;
               u8 leadership;
               u16 hpBase;
               u8 hpGrowth;
               u16 strBase;
               u8 strGrowth;
               u16 vitBase;
               u8 vitGrowth;
               u16 intBase;
               u8 intGrowth;
               u16 menBase;
               u8 menGrowth;
               u16 agiBase;
               u8 agiGrowth;
               u16 dexBase;
               u8 dexGrowth;
               u8 luck;
               u8 alignment;
               u8 resist[7];
               u8 movement;
               u16 defaultItems[4];
               u8 lockedSlots;
               u8 attacks[3][2];
               u8 physAttack;
               u8 magicAttack;
               u8 physDefense;
               u8 magicDefense;
               u8 promotions[4];
               u8 baseClass;
               u8 element;
               u8 squadSlots;
} ClassData;
typedef struct {
               signed char *name;
               u8 category;
               u8 element;
               u8 rank;
               u16 price;
               u8 str;
               u8 intl;
               u8 agi;
               u8 dex;
               u8 vit;
               u8 men;
               s8 luck;
               s8 resist[7];
               u8 growth[2];
               u8 flags;
               u8 inflict;
               u8 protect;
} ItemData;
typedef struct {
               signed char *name;
               u8 kind;
               u8 unk5;
               u16 price;
               u8 target;
               u8 use;
} ConsumableData;
typedef struct {
               signed char *name;
               u8 kind;
               u8 element;
               u8 effect1[4];
               u8 effect2[4];
} AttackData;
typedef struct {
               u8 unk00[0x11];
               u8 cls;
               u8 baseCls;
               u8 level;
               u8 gender;
               u16 maxHp;
               u16 hp;
               u8 element;
               u8 alignment;
               u16 str;
               u16 vit;
               u16 intl;
               u16 men;
               u16 agi;
               u16 dex;
               u8 luck;
               u16 items[4];
               u8 fatigue;
               u8 status;
} Character;
extern ClassData gClassTable[];
extern ItemData gItemTable[];
extern ConsumableData gConsumableTable[];
extern AttackData gAttackTable[];
typedef struct {
               u8 unk0;
               u8 flags;
               u8 members[5];
               u8 pos[5];
               u8 harmony;
               u8 items[10];
               u8 strategy;
               u8 legion;
} SquadRec;
typedef struct {
               Character c;
               u8 unk34[4];
} PlayerChar;
typedef struct {
               u32 flags;
               u8 squad;
               u8 unk05[3];
               f32 x;
               f32 y;
               f32 z;
               s32 unk14;
               u8 unk18[0x34];
               f32 savedX;
               f32 savedY;
               f32 savedZ;
               u8 unk58[0xC];
               s32 unk64;
               u8 unk68[0x18];
               s32 unk80;
               s32 unk84;
               u8 unk88[9];
               u8 unk91;
               u8 unk92;
               u8 unk93[0x15];
               s32 *ids;
               u8 unkAC[0xE];
               u8 unkBA;
} MapUnit;
typedef struct {
    f32 x, y, z;
} Vec3f;
extern u8 gLegions[][11];
extern SquadRec gSquads[];
extern PlayerChar D_80193BC0[];
extern Character D_80195560[];
extern u32 *D_801F0CB0_ovl_101CE0[];
f32 func_8009C780(f32 x, f32 z);
s32 func_801DA228_ovl_101CE0(MapUnit *u);
void func_801DA340_ovl_101CE0(s32 a, Vec3f *out);
extern s32 D_801F3670_ovl_101CE0;
extern u8 D_801EB2D4_ovl_101CE0;
extern u8 D_801EB2D5_ovl_101CE0;
extern s32 D_801EB280_ovl_101CE0;
extern s32 D_801EB284_ovl_101CE0;
extern s32 D_801EB288_ovl_101CE0;
extern void *D_801EB28C_ovl_101CE0;
void func_80079618();
void func_8017C384(s32 h);
void *HFree(void *p);
void func_801B2D2C_ovl_101CE0(MapUnit *u);
typedef struct {
              u16 id;
              u8 equipped;
              u8 count;
} StockEntry;
extern StockEntry D_80196B00[];
extern StockEntry D_80193AC0[];
s32 guRandom(void);
void osSyncPrintf(const signed char *fmt, ...);
extern u8 D_800E7A90[];
extern s32 D_801F1070_ovl_101CE0;
extern MapUnit *D_801F0D28_ovl_101CE0;
f32 func_801A63FC(s32 slot, f32 x, f32 z);
extern f32 D_801F0D98_ovl_101CE0;
extern f32 D_801F0D9C_ovl_101CE0;
extern f32 D_801F0DA0_ovl_101CE0;
extern f32 D_801F0DA4_ovl_101CE0;
extern inline s32 func_801D92D0_ovl_101CE0(MapUnit *u) {
    u8 sq = u->squad;
    s32 leader = gSquads[sq].members[0];
    u16 hp;
    if (leader == 0 || leader >= 100) {
        return 0;
    }
    if (sq < 30) {
        hp = D_80193BC0[leader].c.hp;
    } else {
        hp = D_80195560[leader].hp;
    }
    return hp != 0;
}
extern inline s32 func_801D935C_ovl_101CE0(MapUnit *u) {
    u8 sq = u->squad;
    s32 leader = gSquads[sq].members[0];
    u8 st;
    if (leader == 0 || leader >= 100) {
        return 0;
    }
    if (sq < 30) {
        st = D_80193BC0[leader].c.status & 4;
    } else {
        st = D_80195560[leader].status & 4;
    }
    return st != 0;
}
s32 func_801D93EC_ovl_101CE0(MapUnit *u);
Character *func_801DA0A8_ovl_101CE0(MapUnit *u);
s32 func_801DA1AC_ovl_101CE0(MapUnit *u);
s32 func_801DA210_ovl_101CE0(s32 a);
void func_801DA478_ovl_101CE0(s32 gx, s32 gz, f32 *o);
s32 func_801DA52C_ovl_101CE0(MapUnit *u);
typedef struct {
    u8 unk00[0xA8];
    s32 *ids;
} DB424Arg;
void ChaosFrameOnCapture(s32 loc);
s32 func_801DBE74_ovl_101CE0(MapUnit *u);
s32 func_801DC054_ovl_101CE0(void);
void LocationGetStageValue(u16 loc, s8 *value, u8 *side);
void func_801DC910_ovl_101CE0(void);
void func_801DCC48_ovl_101CE0(SquadRec *sq);
void func_801DCCC4_ovl_101CE0(void);
s32 func_801DCD40_ovl_101CE0(MapUnit *u, u8 item);
s32 func_801DCEE4_ovl_101CE0(void);
s32 func_801DCF20_ovl_101CE0(void) {
    if (func_8016FEF4(5)) {
        return 1;
    }
    if (func_8016FEF4(6)) {
        return 1;
    }
    if (func_8016FEF4(7)) {
        return 1;
    }
    if (func_8016FEF4(8)) {
        return 1;
    }
    if (func_8016FEF4(9)) {
        return 1;
    }
    if (func_8016FEF4(10)) {
        return 1;
    }
    return 0;
}
s32 func_801DCF90_ovl_101CE0(MapUnit *u);
s32 func_801DD208_ovl_101CE0(void);
s32 func_801DD244_ovl_101CE0(void);
s32 func_801DD280_ovl_101CE0(u32 a);
void func_801DD530_ovl_101CE0(void);
void func_801DDDA8_ovl_101CE0(void);
void func_801DE57C_ovl_101CE0(void);
