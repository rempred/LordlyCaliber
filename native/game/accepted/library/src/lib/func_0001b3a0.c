typedef unsigned int u32;
extern void (*D_800A9E8C)(void *);
u32 osSetIntMask(u32 mask);
void func_0001b3a0(void (*func)(void *)) {
    u32 mask;
    mask = osSetIntMask(1);
    D_800A9E8C = func;
    osSetIntMask(mask);
}
