typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
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
s32 func_00195410_helper(u16 id);
s32 func_0020C3B4(BattleUnit *u) {
    s32 r;
    if ((u8)(func_00195410_helper(u->items[0]) - 14) < 2) {
        r = 1;
    } else if ((u8)(func_00195410_helper(u->items[1]) - 14) < 2) {
        r = 1;
    } else if ((u8)(func_00195410_helper(u->items[2]) - 14) < 2) {
        r = 1;
    } else {
        r = (u8)(func_00195410_helper(u->items[3]) - 14) < 2;
    }
    return r;
}
