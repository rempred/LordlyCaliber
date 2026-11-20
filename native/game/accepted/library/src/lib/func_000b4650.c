typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u16 v[3];
} Cursor3;
typedef struct {
    s32 n;
    s32 unk4;
    s32 x;
    s32 y;
} Bob87;
typedef struct {
    u8 unk0000[0x92];
    u8 b92;
    u8 b93;
    u8 b94;
    u8 b95;
    u8 b96;
    u8 unk0097;
    u8 b98;
    u8 unk0099[0xD];
    u16 sp;
    Cursor3 stack[20];
    Cursor3 cur;
    u8 unk0126[6];
    u16 h12C;
    u8 unk012E[0xF76];
    Bob87 bob[8];
    u8 unk1124[0x5C];
    u8 rows[1][0x36];
} Menu87;
extern Menu87 *D_80196AF8;
extern s16 D_801EF1BC[];
void func_000b4650(s32 x, s32 y, s32 i) {
    Bob87 *b = &D_80196AF8->bob[i];
    s32 k = b->n++;
    s32 v = D_801EF1BC[k];
    if (v == 99) {
        b->n = 2;
        v = D_801EF1BC[1];
    }
    b->x = x;
    b->y = y + v;
}
