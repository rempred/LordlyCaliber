typedef unsigned short u16;
typedef signed int s32;
void *D_8017F334(u16 id);
void D_8017C2FC(void *p);
void func_000016C4(void *p);
extern void *D_8022A260[];
void func_001BDCF8(void) {
    void *o = D_8017F334(0x5100);
    s32 i;
    s32 n;
    if (o != ((void *)0)) {
        D_8017C2FC(o);
    }
    n = 34;
    for (i = 0; i < n; i++) {
        if (D_8022A260[i] != ((void *)0)) {
            func_000016C4(D_8022A260[i]);
            D_8022A260[i] = ((void *)0);
        }
    }
}
