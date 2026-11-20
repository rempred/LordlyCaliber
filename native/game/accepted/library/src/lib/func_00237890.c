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
u16 D_801C8FB8(BattleUnit *u);
u8 D_8016F580(u16 id);
u8 D_8016F598(u16 id);
s32 D_801CD19C(s32 a, s32 cls, s32 flag);
u8 func_00237890(BattleUnit *a, BattleUnit *b) {
    u8 inflict;
    u8 protect;
    u16 id = D_801C8FB8(a);
    inflict = 0;
    protect = 0;
    if (id != 0) {
        inflict = D_8016F580(id);
    }
    if (b->items[0] != 0) {
        protect = D_8016F598(b->items[0]);
    }
    if (b->items[1] != 0) {
        protect |= D_8016F598(b->items[1]);
    }
    if (b->items[2] != 0) {
        protect |= D_8016F598(b->items[2]);
    }
    if (b->items[3] != 0) {
        protect |= D_8016F598(b->items[3]);
    }
    if (D_801CD19C(b->unk48, b->cls, 4)) {
        protect |= 2;
    }
    if (D_801CD19C(b->unk48, b->cls, 8)) {
        protect |= 4;
    }
    if (D_801CD19C(b->unk48, b->cls, 2)) {
        protect |= 8;
    }
    if (D_801CD19C(b->unk48, b->cls, 0x10)) {
        protect |= 0x20;
    }
    inflict &= ~protect;
    return inflict;
}
