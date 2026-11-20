typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u16 g_func_001957D0_normal_slots[];
u8 func_000b86c4(void) {
    s32 i;
    for (i = 0; i < 120; i++) {
        if (g_func_001957D0_normal_slots[i] == 0) {
            break;
        }
    }
    return i + 100;
}
