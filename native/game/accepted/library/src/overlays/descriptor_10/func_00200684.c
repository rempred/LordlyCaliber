typedef signed int s32;
extern void *D_801D0748[];
extern void *D_801D074C;
extern void *D_801D0750;
extern void *D_801D0754;
void bzero(void *p, s32 n);
void *func_000016C4(void *p);
void D_801C5D64(void);
void func_00200684(void) {
    func_000016C4(D_801D0748[0]);
    func_000016C4(D_801D074C);
    func_000016C4(D_801D0750);
    func_000016C4(D_801D0754);
    bzero(D_801D0748, 0x1C);
    D_801C5D64();
}
