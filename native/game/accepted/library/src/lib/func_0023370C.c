typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk00[0x11];
    u8 cls;
    u8 baseCls;
    u8 level;
    u8 gender;
    u16 maxHp;
    u16 hp;
    u8 element;
    u8 alignment;
    u16 str;
    u16 vit;
    u16 intl;
    u16 men;
    u16 agi;
    u16 dex;
    u8 luck;
    u16 items[4];
    u8 fatigue;
    u8 status;
} Character;
typedef struct {
    u8 unk0;
    u8 flags;
    u8 members[5];
    u8 pos[5];
    u8 harmony;
    u8 items[10];
    u8 strategy;
    u8 legion;
} SquadRec;
typedef struct {
    Character c;
    u8 unk34[4];
} PlayerChar;
extern SquadRec g_func_001957D0_source_records[];
extern PlayerChar D_80193BC0_copy[];
extern Character D_80195560_copy[];
u8 *D_801E14C0(s32 side);
s32 func_0023370C(void) {
    u8 *a = D_801E14C0(0);
    u8 *b = D_801E14C0(1);
    if (a != ((void *)0) && b == ((void *)0)) {
        return 0;
    }
    if (a == ((void *)0) && b != ((void *)0)) {
        return 1;
    }
    return D_80193BC0_copy[g_func_001957D0_source_records[a[2]].members[0]].c.intl < D_80195560_copy[g_func_001957D0_source_records[b[2]].members[0]].intl;
}
