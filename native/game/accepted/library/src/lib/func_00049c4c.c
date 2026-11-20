extern void *D_8018FDC0;
void func_0005a0f4(void);
void func_00049c4c(void *list) {
    void *old = D_8018FDC0;
    D_8018FDC0 = list;
    func_0005a0f4();
    D_8018FDC0 = old;
}
