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
s32 func_0020C014(BattleUnit *u);
BattleUnit *func_0020C478(u32 i);
extern inline s32 func_0020C014(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return ((u->flags >> 8) ^ 1) & 1;
}
s32 func_0020C050(u8 a, u8 b) {
    return func_0020C014(func_0020C478(a)) ^ func_0020C014(func_0020C478(b));
}
BattleUnit *func_0020C478(u32 i);
