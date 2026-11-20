typedef unsigned char u8;
typedef unsigned int u32;
u8 boot_status_flag_clear(void);
extern u8 D_800AEF9A;
u32 osSetIntMask(u32 mask);
u8 boot_status_flag_clear(void) {
    u32 mask = osSetIntMask(1);
    u8 v = D_800AEF9A & ~5;
    D_800AEF9A = v;
    osSetIntMask(mask);
    return v;
}
