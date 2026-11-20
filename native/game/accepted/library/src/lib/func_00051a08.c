typedef unsigned short u16;
typedef signed int s32;
extern u16 g_state_slot_status;
s32 D_8019C67C(void);
extern u16 g_state_slot_status;
void func_00051a08(void) {
    if (D_8019C67C()) {
        g_state_slot_status = 9;
    }
}
