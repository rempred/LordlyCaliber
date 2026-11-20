typedef unsigned short u16;
extern u16 g_state_slot_status;
extern u16 D_8018F1A2;
void func_00048294(void);
void func_00048024(void);
void func_000466f4(void);
void func_0004eb90(void) {
    func_00048294();
    func_00048024();
    func_000466f4();
    g_state_slot_status = D_8018F1A2;
}
