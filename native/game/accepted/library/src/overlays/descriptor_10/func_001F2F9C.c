typedef unsigned char u8;
typedef signed int s32;
extern u8 D_801976DC;
typedef struct {
    u8 b[0x38];
} PChar1F;
typedef struct {
    u8 b[0x34];
} EChar1F;
extern PChar1F D_80193BC0_copy[];
extern EChar1F D_80195560_copy[];
extern u8 D_801976E8;
extern signed char D_801CE890[];
s32 func_0020C014(u8 *u);
s32 D_8016E1DC(u8 a, u8 b);
s32 func_000238B0(signed char *buf, const signed char *fmt, ...);
void func_001F2E48(signed char *s, s32 side);
void func_001F2F9C(u8 *u, u8 *a) {
    signed char buf[64];
    u8 *rec;
    s32 player;
    if (func_0020C014(u)) {
        s32 sq = D_801976DC;
        player = sq < 30;
    } else {
        s32 sq = D_801976E8;
        player = sq < 30;
    }
    if (player) {
        rec = D_80193BC0_copy[u[0xF6]].b;
    } else {
        rec = D_80195560_copy[u[0xF6]].b;
    }
    func_000238B0(buf, D_801CE890, rec, D_8016E1DC(a[0x4B], a[0x4F]));
    func_001F2E48(buf, func_0020C014(u));
}
