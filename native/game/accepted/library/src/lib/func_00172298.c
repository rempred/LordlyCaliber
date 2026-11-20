typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
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
extern u8 D_8021AFB2;
extern s8 D_8021AFB3;
extern u8 D_8021AFB4;
extern Ctx17 *D_8021AA70;
void func_00172298(void);
void *func_0000978C(u32 handle, ...);
void func_00172298(void) {
    if (D_8021AA70->w70 == 0) {
        D_8021AA70->w70 = (s32)func_0000978C(0x218E9B4, 0x16, -3, 0);
        D_8021AA70->w74 = (s32)func_0000978C(0x218E9B4, 0x16, -3, D_8021AFB4 + 0x14);
        D_8021AA70->w78 = (s32)func_0000978C(0x218E9B4, 0x16, -3, D_8021AFB2 + 1);
        D_8021AA70->w7C = (s32)func_0000978C(0x218E9B4, 0x16, -3, D_8021AFB3 + 0xC);
    }
}
