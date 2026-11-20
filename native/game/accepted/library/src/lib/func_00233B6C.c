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
s32 D_801C8E64(BattleUnit *u);
s32 D_801C8E80(BattleUnit *u);
s32 D_801C8E9C(BattleUnit *u);
s32 D_801C8EB8(BattleUnit *u);
s32 func_00235754(BattleUnit *a, BattleUnit *b);
s32 func_00235BB0(BattleUnit *a, BattleUnit *b);
s32 func_00235DC8(BattleUnit *a, BattleUnit *b);
BattleUnit *func_00233B6C(BattleUnit *self, BattleUnit **list, s32 n, u8 mode) {
    s32 best = -9999;
    s32 bestIdx = -1;
    s32 i;
    s32 v;
    for (i = 0; i < n; i++) {
        v = func_00235754(list[i], ((void *)0)) + func_00235DC8(list[i], ((void *)0)) +
            func_00235BB0(list[i], ((void *)0));
        v *= list[i]->actsMax - list[i]->actsUsed;
        v++;
        if (mode != 0 && D_801C8E9C(list[i])) {
            v = 0;
        } else if (mode == 8 && D_801C8E64(list[i])) {
            v = 0;
        } else if (mode == 10 && D_801C8EB8(list[i])) {
            v = 0;
        } else if (mode == 4 && D_801C8E80(list[i])) {
            v = 0;
        }
        if (v == best) {
            best = v - (rand() & 1);
        }
        if (best < v) {
            best = v;
            bestIdx = i;
        }
    }
    if (bestIdx < 0) {
        return ((void *)0);
    }
    return list[bestIdx];
}
