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
BattleUnit *D_801C8FE8(s32 i);
void func_00237D5C(s32 i, s32 *x, s32 *y) {
    if (i < 20) {
        BattleUnit *u = D_801C8FE8(i);
        *x = u->lane;
        *y = u->unk58;
    } else if (i < 47) {
        i -= 20;
        *x = i % 3;
        *y = i / 3;
    } else if (i < 87) {
        i -= 47;
        *x = i % 4;
        *y = i / 4;
    } else {
        *x = -1;
        *y = -1;
    }
}
