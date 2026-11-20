typedef signed short s16;
typedef unsigned short u16;
extern u16 g_state_slot_status;
s16 D_801B5C24(void);
void func_0004df9c(void) {
    if (D_801B5C24()) {
        g_state_slot_status = 0xFFFD;
    }
}
