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
void func_000b4700(s16 *x, s16 *y, s16 *w, s16 *h) {
    s32 mode = *(u16 *)((u8 *)D_80196AF8 + 0x182);
    u8 sub;
    if (mode == 3) {
        sub = *((u8 *)D_80196AF8 + 0x14D);
        if (sub == 1) {
            mode = 2;
        }
    } else if (mode == 4) {
        sub = *((u8 *)D_80196AF8 + 0x14D);
        if (sub == 2) {
            mode = 3;
        } else if (sub == 1) {
            mode = 2;
        }
    }
    switch (mode) {
        case 0:
        case 1:
        case 2:
            *x = 0x18;
            *y = 0x14;
            *w = 0x128;
            *h = 0x38;
            break;
        case 3:
            *x = 0x18;
            *y = 0x14;
            *w = 0x128;
            *h = 0x38;
            break;
        case 4:
            *x = 0x9A;
            *y = 0x14;
            *w = 0x128;
            *h = 0x42;
            break;
    }
}
