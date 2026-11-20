typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
extern u8 D_80196A30[];
void func_00047ee0(s32 a, s32 b);
void func_000424bc(void *d);
void func_0004260c(void *d);
void func_000491e0(s16 n) {
    s16 i;
    if (n >= 0) {
        func_00047ee0(0x43, 3);
        for (i = 0; i < n; i++) {
            func_000424bc(D_80196A30);
            func_00047ee0(0x43, 3);
        }
    } else {
        for (n++; n != 1; n++) {
            func_0004260c(D_80196A30);
        }
    }
}
