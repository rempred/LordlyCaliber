typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct {
    s16 h0;
    u8 unk02[2];
    s32 w4;
    u8 unk08[4];
    s32 wC;
    u8 unk10[0x74];
    s32 x84;
    u8 x88;
    u8 x89;
    u8 x8A;
    u8 x8B;
} Widget3390;
extern Widget3390 *D_801B3390;
extern s32 D_801B3DE0[];
void func_000fb7dc(s32 i) {
    D_801B3390->x88 = 1;
    D_801B3390->x89 = 0xFF;
    D_801B3390->x84 = D_801B3DE0[i];
    D_801B3390->x8B = 0;
    D_801B3390->x8A = 0;
}
