typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
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
typedef struct {
    u32 flags;
    u8 squad;
    u8 unk05[3];
    f32 x;
    f32 y;
    f32 z;
    s32 unk14;
    u8 unk18[0x34];
    f32 savedX;
    f32 savedY;
    f32 savedZ;
    u8 unk58[0xC];
    s32 unk64;
    u8 unk68[0x18];
    s32 unk80;
    s32 unk84;
    u8 unk88[9];
    u8 unk91;
    u8 unk92;
    u8 unk93[0x15];
    s32 *ids;
    u8 unkAC[0xE];
    u8 unkBA;
} MapUnit;
extern SquadRec g_func_001957D0_source_records[];
extern PlayerChar D_80193BC0_copy[];
extern Character D_80195560_copy[];
inline s32 func_0012DA9C(MapUnit *u) {
    u8 sq = u->squad;
    s32 leader = g_func_001957D0_source_records[sq].members[0];
    u8 st;
    if (leader == 0 || leader >= 100) {
        return 0;
    }
    if (sq < 30) {
        st = D_80193BC0_copy[leader].c.status & 4;
    } else {
        st = D_80195560_copy[leader].status & 4;
    }
    return st != 0;
}
