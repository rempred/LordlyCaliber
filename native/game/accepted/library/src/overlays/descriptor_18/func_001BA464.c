typedef unsigned short u16;
typedef unsigned int u32;
extern u16 D_8022A218[];
extern u16 *D_80229CB0[];
extern u16 D_8022A822;
void func_001BA464(void) {
    u32 i;
    for (i = 0; i < 12; i++) {
        *D_80229CB0[i] = D_8022A218[i] & ~D_8022A822;
    }
}
