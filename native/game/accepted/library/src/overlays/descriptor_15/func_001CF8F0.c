typedef unsigned char u8;
typedef signed int s32;
extern s32 D_801B8094;
extern u8 g_resource_state_byte_0;
extern u8 g_resource_state_byte_1;
extern u8 g_resource_state_byte_2;
extern s32 g_resource_state_word;
u8 D_8016CD30(void);
void func_001CF65C(void);
void func_001CF8F0(void) {
    if (D_8016CD30()) {
        g_resource_state_byte_2 = 1;
        g_resource_state_byte_1 = 1;
        g_resource_state_byte_0 = 1;
        if (g_resource_state_word == 0) {
            g_resource_state_word = (D_801B8094 + 0x3F) & ~0x3F;
        }
        func_001CF65C();
    }
}
