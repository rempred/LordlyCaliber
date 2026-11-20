typedef unsigned int u32;
extern void *D_800A9E84;
u32 osSetIntMask(u32 mask);
void func_00019dd0(void *func) {
    u32 mask;
    mask = osSetIntMask(1);
    D_800A9E84 = func;
    osSetIntMask(mask);
}
