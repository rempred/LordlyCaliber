typedef signed int s32;
void *func_00001330(s32 size);
void bzero(void *p, s32 n);
void func_00294E04(void *p);
void bzero(void *p, s32 n);
void bzero(void *p, s32 n);
void *func_00294FAC(void) {
    void *p = func_00001330(0x500);
    bzero(p, 0x500);
    func_00294E04(p);
    return p;
}
