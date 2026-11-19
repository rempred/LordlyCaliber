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
typedef struct BattleObj {
               u8 unk00[0x18];
               s32 unk18;
               u8 unk1C[0x28];
               s32 unk44;
} BattleObj;
typedef struct BattleUnit {
               struct BattleObj *obj;
               u8 unk04[0x1C];
               u16 hp;
               u16 maxHp;
               u16 str;
               u16 vit;
               u8 unk28[6];
               u16 dex;
               u8 unk30;
               u8 unk31[2];
               u8 unk33;
               u8 unk34[2];
               u16 items[4];
               u8 unk3E;
               u8 unk3F;
               u32 flags;
               u8 unk44[4];
               s32 unk48;
               s32 cls;
               s32 unk50;
               s32 lane;
               s32 unk58;
               u8 unk5C[0xC];
               s32 target;
               s32 actsUsed;
               s32 actsMax;
               u8 unk74[4];
               u8 actionKind;
               u8 unk79[3];
               s32 attack;
               s32 unk80;
               u8 unk84[0x10];
               s32 unk94;
               u8 unk98[0x60];
} BattleUnit;
typedef struct {
                u8 unk000[0x1C4];
                BattleUnit units[20];
} BattleState;
s32 func_8009C7CC(void);
s32 func_801C8B84(BattleUnit *u);
s32 func_801C8BC0(u8 a, u8 b);
s32 func_801C8C3C(BattleUnit *u);
s32 func_801C8C58(BattleUnit *u);
s32 func_801C8D24(BattleUnit *u);
s32 func_801C8E30(BattleUnit *u);
s32 func_801C8E64(BattleUnit *u);
s32 func_801C8E80(BattleUnit *u);
s32 func_801C8E9C(BattleUnit *u);
s32 func_801C8EB8(BattleUnit *u);
u16 func_801C8FB8(BattleUnit *u);
BattleUnit *func_801C8FE8(s32 i);
u8 func_801C9FA4(s32 a);
s32 func_801CA038(BattleUnit *u, s32 amount);
s32 func_801CA29C(BattleUnit *u);
s32 func_801CA384(BattleUnit *u);
u8 func_8016E458(u8 a);
s32 func_8016F5E0(u16 a);
u16 func_8016FB50(u16 a, u16 b, u16 c, u16 d);
s32 func_801F0514_ovl_22A280(BattleUnit **out, s32 start, s32 lane);
s32 func_801F14E8_ovl_22A280(BattleUnit *u);
s32 func_801F2484_ovl_22A280(BattleUnit *a, BattleUnit *b);
s32 func_801F28E0_ovl_22A280(BattleUnit *a, BattleUnit *b);
s32 func_801F2AF8_ovl_22A280(BattleUnit *a, BattleUnit *b);
s32 BattleCalcDamage(BattleUnit *a, BattleUnit *b, f32 power, s32 c, s32 d, s32 e);
extern s8 D_801F6A30_ovl_22A280[][3];
s32 func_801F4480_ovl_22A280(BattleUnit *a, BattleUnit *b, s32 c, s32 d, s32 e);
BattleUnit *func_801F0A3C_ovl_22A280(BattleUnit *self, BattleUnit **list, s32 n);
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
extern SquadRec gSquads[];
extern PlayerChar D_80193BC0[];
extern Character D_80195560[];
extern u8 D_801976DC[];
extern u8 D_801976E8[];
extern u8 D_80190F80[];
extern u8 D_801953F0[];
u8 *func_801E14C0(s32 side);
s32 func_801C9FB4(u16 a, u16 b);
s32 func_801C8AC0(s32 a);
f32 func_8007FE70(f32 *a, f32 *b);
s32 func_801C8D84(BattleUnit *u);
s32 func_801C8B68(BattleUnit *u);
u8 func_8016F580(u16 id);
u8 func_8016F598(u16 id);
typedef struct {
    u8 mode;
    u8 unk1[3];
    u8 flags;
} BattleMode;
extern BattleMode D_801976D9;
extern u8 D_801976E9;
s32 func_8016D2FC(u8 a, u8 b);
s8 func_8016F65C(u16 id);
s8 func_8016F6AC(u16 id);
s8 func_8016F7EC(u16 id);
u8 func_8016D3A4(u8 a, u8 cls);
f32 func_801F1B98_ovl_22A280(BattleUnit *a, BattleUnit *b);
s32 func_8016F9F0(u16 id);
f32 func_801F1884_ovl_22A280(BattleUnit *a, BattleUnit *b);
extern inline s16 BattleUnitWeaponBonus(BattleUnit *u) {
    u16 id;
    if (u == ((void *)0)) {
        return 0;
    }
    id = func_801C8FB8(u);
    if (id == 0) {
        return 0;
    }
    return func_8016F65C(id);
}
extern inline s32 BattleUnitItemSum7EC(BattleUnit *u) {
    s16 s;
    if (u == ((void *)0)) {
        return 0;
    }
    s = u->items[0] != 0 ? func_8016F7EC(u->items[0]) : 0;
    if (u->items[1] != 0) {
        s += func_8016F7EC(u->items[1]);
    }
    if (u->items[2] != 0) {
        s += func_8016F7EC(u->items[2]);
    }
    if (u->items[3] != 0) {
        return s + func_8016F7EC(u->items[3]);
    }
    return s;
}
extern inline s32 BattleUnitItemSum6AC(BattleUnit *u) {
    s16 s;
    if (u == ((void *)0)) {
        return 0;
    }
    s = u->items[0] != 0 ? func_8016F6AC(u->items[0]) : 0;
    if (u->items[1] != 0) {
        s += func_8016F6AC(u->items[1]);
    }
    if (u->items[2] != 0) {
        s += func_8016F6AC(u->items[2]);
    }
    if (u->items[3] != 0) {
        return s + func_8016F6AC(u->items[3]);
    }
    return s;
}
extern inline s32 BattleUnitItemSum65C(BattleUnit *u) {
    s16 s;
    if (u == ((void *)0)) {
        return 0;
    }
    s = u->items[0] != 0 ? func_8016F65C(u->items[0]) : 0;
    if (u->items[1] != 0) {
        s += func_8016F65C(u->items[1]);
    }
    if (u->items[2] != 0) {
        s += func_8016F65C(u->items[2]);
    }
    if (u->items[3] != 0) {
        return s + func_8016F65C(u->items[3]);
    }
    return s;
}
BattleUnit *func_801CA2E0(s32 lane, s32 row);
s32 func_801CD19C(s32 a, s32 cls, s32 flag);
s32 func_801C8B08(BattleUnit *u);
u8 func_8016FF5C(u8 a, u8 b);
u16 func_8016DF88(u8 a, u8 cls);
u16 func_8016DFDC(u8 a, u8 cls);
u16 func_8016E030(u8 a, u8 cls);
u16 func_8016E084(u8 a, u8 cls);
u8 func_8016E338(u8 a, u8 cls, u8 c);
s32 func_801C8BA4(BattleUnit *u);
s32 func_801DF2AC(s32 *p, s32 a, s32 b);
void func_801D9F8C(s32 a, s32 b, BattleUnit *u, s32 c, s32 d);
void func_801D9F60(s32 a, s32 b, BattleUnit *u, s32 c);
extern BattleState *D_801CE8BC;
s32 func_801BE138(s32 a, s32 cls, s32 c, s32 d);
void func_801E15E0(BattleUnit *u, s32 cls);
s32 func_801F0310_ovl_22A280(s32 side);
s32 func_801F043C_ovl_22A280(void);
BattleUnit *func_801F089C_ovl_22A280(BattleUnit *self, BattleUnit **list, s32 n, u8 mode);
s32 func_801F0C04_ovl_22A280(BattleUnit *u);
s32 func_801F0C68_ovl_22A280(BattleUnit *u);
s32 func_801F0CC4_ovl_22A280(BattleUnit *u);
s32 func_801F0E1C_ovl_22A280(BattleUnit *self);
extern inline void func_801F0F5C_ovl_22A280(BattleUnit *u, s32 cls) {
    BattleObj *o;
    s32 v;
    s32 x;
    if (u->cls == u->unk48) {
        u->unk48 = cls;
    }
    u->cls = cls;
    u->items[0] = func_8016DF88(u->unk48, cls);
    u->items[1] = func_8016DFDC(u->unk48, cls);
    u->items[2] = func_8016E030(u->unk48, cls);
    u->items[3] = func_8016E084(u->unk48, cls);
    u->actsMax = func_8016E338(u->unk48, u->cls, func_801C9FA4(u->unk58));
    x = func_801C8B68(u);
    v = func_801BE138(u->unk48, cls, x, func_801C8BA4(u));
    o = u->obj;
    u->unk50 = v;
    o->unk44 = v;
    o->unk18 = v;
    func_801E15E0(u, cls);
}
s32 func_801F11E4_ovl_22A280(BattleUnit *u, s32 d);
s8 func_801F1344_ovl_22A280(BattleUnit *u);
s32 func_801F139C_ovl_22A280(BattleUnit *a, BattleUnit *b);
s32 func_801F1444_ovl_22A280(BattleUnit *a, BattleUnit *b);
f32 func_801F22CC_ovl_22A280(BattleUnit *u) {
    f32 base = func_801F14E8_ovl_22A280(u);
    s32 b = 0;
    if (u != ((void *)0)) {
        if (u->unk3E >= 70) {
            if (u->unk3E < 80) {
                b = 5;
            } else if (u->unk3E < 90) {
                b = 18;
            } else {
                b = 40;
            }
        }
    }
    return base - b;
}
s32 func_801F356C_ovl_22A280(u8 a, u8 b);
s32 func_801F4480_ovl_22A280(BattleUnit *a, BattleUnit *b, s32 c, s32 d, s32 e);
s32 func_801F4540_ovl_22A280(BattleUnit *a, BattleUnit *b);
u8 func_801F45C0_ovl_22A280(BattleUnit *a, BattleUnit *b);
s32 func_801F46DC_ovl_22A280(s32 effect, BattleUnit *u);
void func_801F4A8C_ovl_22A280(s32 i, s32 *x, s32 *y);
s32 func_801F54C8_ovl_22A280(BattleUnit *u);
s32 func_801F67D4_ovl_22A280(BattleUnit *u, u32 f);
s32 func_801F68A4_ovl_22A280(u8 *a, u8 *b);
s32 func_801F68B4_ovl_22A280(u8 *a, u8 *b);
