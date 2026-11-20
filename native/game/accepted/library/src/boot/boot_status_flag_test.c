typedef unsigned char u8;
typedef unsigned int u32;
u8 boot_status_flag_test(void);
extern u8 D_800AEF9A;
u32 osSetIntMask(u32 mask);
u8 boot_status_flag_test(void) {
    u32 mask = osSetIntMask(1);
    u8 v = D_800AEF9A;
    osSetIntMask(mask);
    return v & 4;
}
