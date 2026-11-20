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
u8 D_8016F598(u16 id);
s32 D_801CD19C(s32 a, s32 cls, s32 flag);
s32 func_002379AC(s32 effect, BattleUnit *u) {
    u8 p = 0;
    s32 r;
    if (u->items[0] != 0) {
        p = D_8016F598(u->items[0]);
    }
    if (u->items[1] != 0) {
        p |= D_8016F598(u->items[1]);
    }
    if (u->items[2] != 0) {
        p |= D_8016F598(u->items[2]);
    }
    if (u->items[3] != 0) {
        p |= D_8016F598(u->items[3]);
    }
    if (D_801CD19C(u->unk48, u->cls, 4)) {
        p |= 2;
    }
    if (D_801CD19C(u->unk48, u->cls, 8)) {
        p |= 4;
    }
    if (D_801CD19C(u->unk48, u->cls, 2)) {
        p |= 8;
    }
    if (D_801CD19C(u->unk48, u->cls, 0x10)) {
        p |= 0x20;
    }
    switch ((u32)effect) {
        case 4:
            r = p & 2;
            break;
        case 3:
            r = p & 8;
            break;
        case 8:
            r = p & 4;
            break;
        case 10:
            r = p & 0x20;
            break;
        default:
            return 1;
    }
    if (r != 0) {
        r = 0;
    } else {
        r = 1;
    }
    return r;
}
