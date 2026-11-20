typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
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
extern u16 g_state_slot_status;
void func_00019e10(void);
void func_000fbe2c(Widget3390 *p);
void func_000fb0d0(void);
void func_000fb0d0(void) {
    func_00019e10();
    g_state_slot_status = 0xFFFE;
    func_000fbe2c(D_801B3390);
}
void func_000fbe2c(Widget3390 *w);
