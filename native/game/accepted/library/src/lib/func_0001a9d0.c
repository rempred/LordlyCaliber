typedef unsigned int u32;
extern u32 D_800C4B30;
u32 osSetIntMask(u32 mask);
void func_0001a9d0(void) {
    u32 mask;
    mask = osSetIntMask(1);
    D_800C4B30 = 0;
    osSetIntMask(mask);
}
