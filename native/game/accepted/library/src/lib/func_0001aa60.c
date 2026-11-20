typedef unsigned int u32;
extern void (*D_800A9FE0)(u32);
u32 osSetIntMask(u32 mask);
void func_0001aa60(void (*func)(u32)) {
    u32 mask;
    mask = osSetIntMask(1);
    D_800A9FE0 = func;
    osSetIntMask(mask);
}
