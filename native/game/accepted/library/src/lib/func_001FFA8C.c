void memset_00023780(void *ptr, int size);

extern char D_801D0770[];
extern char D_801D0748[];
extern char D_801D0828[];

void func_001FFA8C(void)
{
    memset_00023780(D_801D0770, 0xA0);
    memset_00023780(D_801D0748, 0x1C);
    memset_00023780(D_801D0828, 5);
}
