typedef signed short s16;
typedef unsigned short u16;
extern u16 g_state_slot_status;
s16 D_8022857C(void);
void func_0004e150(void) {
    if (D_8022857C()) {
        g_state_slot_status = 0xFFFE;
    }
}
