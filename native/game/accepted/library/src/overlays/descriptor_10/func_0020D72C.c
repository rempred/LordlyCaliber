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
extern u8 *D_801CE8BC;
s32 func_0020D72C(BattleUnit *u) {
    s32 i;
    u8 *base;
    if (u == ((void *)0)) {
        return -1;
    }
    base = D_801CE8BC;
    for (i = 0; i < 20; i++) {
        if ((BattleUnit *)(base + 0x1C4 + i * 0xF8) == u) {
            return i;
        }
    }
    return -1;
}
