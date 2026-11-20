typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef float f32;
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
void func_000b362c(f32 *x, f32 *y, s32 n) {
    s32 col = (*(s32 *)((u8 *)D_80196AF8 + 0x2C0) - 1) / 3 - n / 3 + 1;
    s32 row = 2 - n % 3;
    f32 c = col;
    f32 r = row;
    *x = c * 72.0f + r * 30.0f + 0.5f;
    *y = r * 50.0f - c * 20.0f + 0.5f - 72.0f;
}
