typedef signed int s32;
extern void *D_8022A4C0[];
void func_000016C4(void *p);
void func_001BDF2C(void) {
    s32 i;
    s32 n = 6;
    for (i = 0; i < n; i++) {
        func_000016C4(D_8022A4C0[i]);
        D_8022A4C0[i] = ((void *)0);
    }
}
