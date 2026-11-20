typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct {
    u8 unk00[0x5C];
    s16 h5C;
    u8 b5E;
    u8 unk5F;
    s32 w60;
    u8 b64;
    u8 b65;
    u8 unk66[2];
    s32 w68;
    s32 w6C;
    s32 w70;
    s32 w74;
    s32 w78;
    s32 w7C;
} Ctx17;
extern Ctx17 *D_8021AA70;
void D_80079618();
void func_00172364(void) {
    if (D_8021AA70->w70 != 0) {
        D_80079618(0x218E9B4, 0x16, -0x11);
        D_8021AA70->w70 = 0;
        D_8021AA70->w74 = 0;
        D_8021AA70->w78 = 0;
        D_8021AA70->w7C = 0;
    }
}
