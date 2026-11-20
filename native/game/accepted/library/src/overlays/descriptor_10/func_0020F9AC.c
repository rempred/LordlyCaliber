typedef unsigned char u8;
typedef signed int s32;
extern struct { u8 b[12]; } D_801976DC[];
typedef struct {
    u8 unk0[0xD];
    u8 items[10];
    u8 unk17[2];
} SquadRec;
extern SquadRec g_func_001957D0_source_records[];
s32 func_0020F9AC(s32 id) {
    SquadRec *sq = &g_func_001957D0_source_records[D_801976DC[0].b[0]];
    s32 i;
    for (i = 0; i < 10; i++) {
        if (sq->items[i] == id) {
            return 1;
        }
    }
    return 0;
}
