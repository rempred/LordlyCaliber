typedef unsigned short u16;
typedef signed int s32;
extern u16 g_state_slot_status;
void D_80198080(s32 arg);
void func_0004d600(void) {
    D_80198080(0x29);
    g_state_slot_status = 0x8014;
}
