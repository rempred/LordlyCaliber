typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
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
s32 func_00235754(BattleUnit *a, BattleUnit *b);
s32 func_00235BB0(BattleUnit *a, BattleUnit *b);
s32 func_00235DC8(BattleUnit *a, BattleUnit *b);
s32 func_0023697C(BattleUnit *a, BattleUnit *b, f32 power, s32 c, s32 d, s32 e);
s32 func_00237750(BattleUnit *a, BattleUnit *b, s32 c, s32 d, s32 e);
s32 func_00237750(BattleUnit *a, BattleUnit *b, s32 c, s32 d, s32 e) {
    f32 p;
    if ((u8)(a->actionKind - 4) < 2) {
        p = func_00235BB0(a, b);
    } else if (a->actionKind == 3) {
        p = func_00235DC8(a, b);
    } else {
        p = func_00235754(a, b);
    }
    return func_0023697C(a, b, p, c, d, e);
}
