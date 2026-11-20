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
extern struct { u8 b[12]; } D_801976DC[];
extern u8 *D_801CE8BC;
typedef struct {
    u8 unk0[0xD];
    u8 items[10];
    u8 unk17[2];
} SquadRec;
extern SquadRec g_func_001957D0_source_records[];
s32 func_0020C014(BattleUnit *u);
extern inline s32 func_0020C014(BattleUnit *u) {
    if (u == ((void *)0)) {
        return 0;
    }
    return ((u->flags >> 8) ^ 1) & 1;
}
s32 func_0020D814(BattleUnit *u) {
    if (func_0020C014(u)) {
        return *(s32 *)(D_801CE8BC + 0x1C0);
    }
    return g_func_001957D0_source_records[D_801976DC[1].b[0]].unk17[0];
}
