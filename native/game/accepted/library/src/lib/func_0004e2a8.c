typedef unsigned short u16;
typedef signed int s32;
extern u16 g_state_slot_status;
s32 D_801D40B0(void);
void func_0004e2a8(void) {
    if (D_801D40B0() == 100) {
        g_state_slot_status = 0xFFFD;
    }
}
