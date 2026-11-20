extern void *D_80197B18;
extern void *D_80197B1C;
void func_000016C4(void *p);
void func_000466f4(void) {
    void **p = &D_80197B18;
    func_000016C4(*p);
    func_000016C4(D_80197B1C);
    *p = ((void *)0);
    D_80197B1C = ((void *)0);
}
