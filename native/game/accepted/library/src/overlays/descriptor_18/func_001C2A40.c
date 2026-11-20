typedef unsigned short u16;
void *D_8017F334(u16 id);
void D_8017C2FC(void *p);
void func_001C2A40(void) {
    void *o = D_8017F334(0x5500);
    if (o != ((void *)0)) {
        D_8017C2FC(o);
    }
}
