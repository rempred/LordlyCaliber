typedef unsigned short u16;
typedef signed int s32;
extern u16 g_state_slot_status;
extern u16 D_8018F1A2;
void func_00048294(void);
void func_000466f4(void);
void D_801ACB04(void);
void func_0004813c(s32 arg);
void func_0004ded0(void) {
    D_801ACB04();
    func_00048294();
    func_0004813c(1);
    func_000466f4();
    g_state_slot_status = D_8018F1A2;
}
