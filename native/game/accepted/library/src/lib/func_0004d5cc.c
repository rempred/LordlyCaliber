typedef unsigned short u16;
typedef signed int s32;
extern u16 g_state_slot_status;
void D_80198080(s32 arg);
void func_0004d5cc(void) {
    D_80198080(-1);
    g_state_slot_status = 0x800E;
}
