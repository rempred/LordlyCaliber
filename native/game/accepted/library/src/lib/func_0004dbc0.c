typedef unsigned short u16;
typedef signed int s32;
extern u16 g_state_slot_status;
void func_0000377C(void);
s32 D_801EE5E0(void);
void func_0004dbc0(void) {
    s32 v;
    func_0000377C();
    v = D_801EE5E0();
    if (v < 3) {
        if (v > 0) {
            g_state_slot_status = 0xFFFE;
        }
    }
}
