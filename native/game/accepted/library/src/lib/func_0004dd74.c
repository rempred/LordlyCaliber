typedef unsigned short u16;
typedef signed int s32;
extern u16 g_state_slot_status;
s32 D_801A3C80(void);
void func_0004dd74(void) {
    if (D_801A3C80()) {
        g_state_slot_status = 0x8004;
    }
}
