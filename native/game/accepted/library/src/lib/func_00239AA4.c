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
s32 D_801C8B84(BattleUnit *u);
s32 D_801C8C58(BattleUnit *u);
s32 D_801C8B68(BattleUnit *u);
s32 D_801C8B08(BattleUnit *u);
u8 D_8016FF5C(u8 a, u8 b);
s32 func_00239AA4(BattleUnit *u, u32 f) {
    s32 r = 0;
    s32 k = D_801C8B08(u);
    if (f & 0x8000) {
        return 0;
    }
    if (D_801C8B84(u) && (f & 0x400)) {
        r = 1;
    }
    if (D_801C8B68(u) && !(f & 0x400)) {
        r = 1;
    }
    if (k == 0) {
        r = 0;
    }
    if (r) {
        if (k == 0xFF) {
            if (!D_801C8C58(u)) {
                r = 0;
            }
        } else if (!D_8016FF5C(u->unk48, k)) {
            r = 0;
        }
    }
    return r;
}
