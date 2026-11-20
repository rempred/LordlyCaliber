typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
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
s32 rand(void);
s32 D_801C8BC0(u8 a, u8 b);
s32 D_801C8C3C(BattleUnit *u);
s32 D_801C8D24(BattleUnit *u);
s32 D_801C8E30(BattleUnit *u);
BattleUnit *D_801C8FE8(s32 i);
s32 D_801CA038(BattleUnit *u, s32 amount);
s32 D_801CA29C(BattleUnit *u);
s32 func_002340EC(BattleUnit *self) {
    s32 selfIdx = D_801CA29C(self);
    s32 best = -1;
    s32 bestIdx = -1;
    s32 i;
    BattleUnit *u;
    s32 v;
    for (i = 0; i < 20; i++) {
        u = D_801C8FE8(i);
        if (u == ((void *)0)) {
            continue;
        }
        if (D_801C8E30(u)) {
            continue;
        }
        if (D_801C8C3C(u)) {
            continue;
        }
        if (D_801C8BC0(selfIdx, i)) {
            continue;
        }
        if (D_801C8D24(u)) {
            continue;
        }
        v = D_801CA038(u, 999);
        v = v * 100 / (u->hp + v);
        if (best == v) {
            best -= rand() & 1;
        }
        if (best < v) {
            best = v;
            bestIdx = i;
        }
    }
    return bestIdx;
}
