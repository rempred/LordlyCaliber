typedef signed int s32;
extern void *D_801BA6F8;
void *func_00001330(s32 size);
void bzero(void *p, s32 n);
void *func_00001330(s32 size);
void func_001D140C(void) {
    void *p = func_00001330(0x398);
    D_801BA6F8 = p;
    bzero(p, 0x398);
}
