typedef unsigned short u16;
typedef signed int s32;
extern u16 g_state_slot_status;
extern u16 D_8018F1A2;
void func_00048294(void);
void func_000466f4(void);
void func_0004813c(s32 arg);
s32 D_8019BCEC(void);
void func_00048268(void);
extern s32 D_8018FB34;
void func_0004d938(void) {
    switch (D_8019BCEC()) {
        case 1:
            func_00048268();
            func_00048294();
            func_0004813c(1);
            func_000466f4();
            g_state_slot_status = D_8018F1A2;
            break;
        case 2:
            g_state_slot_status = 0x8012;
            break;
        case 3:
            g_state_slot_status = 0x17;
            break;
        case 4:
            switch (D_8018FB34) {
                case 0:
                    g_state_slot_status = 0xB;
                    break;
                case 1:
                    g_state_slot_status = 0xC;
                    break;
            }
            D_8018FB34 = (D_8018FB34 + 1) % 2;
            break;
    }
}
