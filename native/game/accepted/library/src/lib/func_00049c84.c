extern void *D_8018FDC0;
void func_00056adc(void);
void func_00049c84(void *list) {
    void *old = D_8018FDC0;
    D_8018FDC0 = list;
    func_00056adc();
    D_8018FDC0 = old;
}
