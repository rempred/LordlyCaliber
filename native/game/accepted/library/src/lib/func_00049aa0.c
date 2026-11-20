extern void *D_8018FDC0;
void func_00051e10(void);
void *func_00049aa0(void *list) {
    void *old = D_8018FDC0;
    void *r;
    D_8018FDC0 = list;
    func_00051e10();
    r = D_8018FDC0;
    D_8018FDC0 = old;
    return r;
}
