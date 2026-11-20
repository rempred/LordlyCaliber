typedef unsigned int u32;
extern u32 D_800A9E70;
void func_00019e10(void);
u32 osSetIntMask(u32 mask);
void func_0001b360(u32 size) {
    u32 mask;
    func_00019e10();
    mask = osSetIntMask(1);
    D_800A9E70 = size;
    osSetIntMask(mask);
}
