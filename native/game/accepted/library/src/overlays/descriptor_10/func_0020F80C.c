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
s32 func_0020C014(BattleUnit *u);
extern inline s32 func_0020C014(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return ((u->flags >> 8) ^ 1) & 1;
}
extern inline s32 func_0020C0CC(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return (u->flags >> 12) & 1;
}
extern inline s32 func_0020C2C0(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 1;
    }
    return u->flags & 1;
}
extern inline s32 func_0020C32C(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return (u->flags >> 1) & 1;
}
extern inline BattleUnit *BattleGetUnit(u32 i) {
    u8 *base = D_801CE8BC;
    s32 off;
    if (i < 20) {
        off = i * 0xF8;
        if (*(s32 *)(base + off + 0x20C) != 0) {
            return (BattleUnit *)(base + (off + 0x1C4));
        }
    }
    return ((void *)0);
}
s32 func_0020F80C(void) {
    s32 enemy = 0;
    s32 player = 0;
    s32 i;
    BattleUnit *u;
    for (i = 0; i < 20; i++) {
        u = BattleGetUnit(i);
        if (func_0020C2C0(u)) {
            continue;
        }
        if (func_0020C0CC(u)) {
            continue;
        }
        if (func_0020C32C(u)) {
            continue;
        }
        if (func_0020C014(u)) {
            player++;
        } else {
            enemy++;
        }
    }
    return (player == 0) | (enemy == 0);
}
