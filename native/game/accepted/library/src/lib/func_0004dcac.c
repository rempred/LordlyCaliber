typedef unsigned short u16;
typedef signed int s32;
extern u16 g_state_slot_status;
s32 func_00046694(s32 arg);
s32 D_801A1B54(void);
void func_0004dcac(void) {
    if (func_00046694(1) || D_801A1B54()) {
        g_state_slot_status = 0x800A;
    }
}
