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
s32 func_0020C1B4(BattleUnit *u) {
    s32 c;
    s32 r;
    if (u == ((void *)0)) {
        return 0;
    }
    c = u->cls;
    r = 0;
    if ((u32)(c - 45) < 2 || c == 47) {
        r = 1;
    } else if (c == 48) {
        r = 1;
    }
    return r;
}
