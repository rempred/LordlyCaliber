typedef unsigned int u32;
extern void *D_800AA090;
u32 osSetIntMask(u32 mask);
void func_00019e10(void);
void func_00019d90(void *func) {
    u32 mask;
    func_00019e10();
    mask = osSetIntMask(1);
    D_800AA090 = func;
    osSetIntMask(mask);
}
