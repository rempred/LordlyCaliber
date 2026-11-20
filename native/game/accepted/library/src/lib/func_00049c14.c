extern void *D_8018FDC0;
void func_00055390(void);
void func_00049c14(void *list) {
    void *old = D_8018FDC0;
    D_8018FDC0 = list;
    func_00055390();
    D_8018FDC0 = old;
}
