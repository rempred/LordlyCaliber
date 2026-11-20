typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct {
    u8 unk00[1];
    u8 flags;
    u8 members[5];
    u8 unk07[0x12];
} SquadRec;
extern u8 D_801976DC;
extern SquadRec g_func_001957D0_source_records[];
inline s32 func_0027a020(s16 slot) {
    SquadRec *sq = &g_func_001957D0_source_records[D_801976DC];
    u8 m = sq->members[slot];
    if (m < 100) {
        return 0;
    }
    return m - 99;
}
