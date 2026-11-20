typedef unsigned char u8;
typedef struct {
    u8 unk0;
    u8 flags;
    u8 rest[0x17];
} SquadRec;
typedef struct {
    u8 unk0[4];
    u8 squad;
} MapUnit12;
extern SquadRec g_func_001957D0_source_records[];
void func_00129948(MapUnit12 *u) {
    SquadRec *s = &g_func_001957D0_source_records[u->squad];
    s->flags |= 4;
}
