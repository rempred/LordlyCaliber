typedef unsigned char u8;
typedef signed int s32;
extern s32 D_801B8090;
extern s32 D_801B8094;
extern u8 g_resource_state_byte_0;
extern u8 g_resource_state_byte_1;
extern u8 g_resource_state_byte_2;
extern s32 g_resource_state_word;
extern u8 *D_801B8088;
extern u8 *D_801B808C;
extern u8 D_800E7A0C;
void func_00019e10(void);
void boot_mode_message_select(s32 a);
void func_00019fc0(void);
void func_00019eb0(void *p, s32 n);
void D_8016CD50(s32 a);
extern u8 *D_800A8204[];
extern u8 *D_800C4BB8;
void func_000016C4(void *p);
void func_001CF588(void) {
    u8 **raw;
    func_00019e10();
    boot_mode_message_select(2);
    func_00019fc0();
    func_00019eb0(D_800A8204, 3);
    D_800E7A0C = 1;
    D_800C4BB8 = D_800A8204[0];
    D_8016CD50(0);
    raw = &D_801B8088;
    D_801B8090 = 1;
    func_000016C4(raw[0]);
    raw[0] = ((void *)0);
    func_000016C4(D_801B808C);
    D_801B808C = ((void *)0);
    if (D_801B8094 != 0) {
        func_000016C4((void *)D_801B8094);
        g_resource_state_word = 0;
        D_801B8094 = 0;
        g_resource_state_byte_2 = 0;
        g_resource_state_byte_1 = 0;
        g_resource_state_byte_0 = 0;
    }
}
