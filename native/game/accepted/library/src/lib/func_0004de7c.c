typedef signed short s16;
typedef unsigned short u16;
extern u16 g_state_slot_status;
s16 D_801ACC04(void);
void func_0004de7c(void) {
    if (D_801ACC04() == -1) {
        g_state_slot_status = 0xFFFE;
    }
}
