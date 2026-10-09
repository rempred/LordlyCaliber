typedef signed int s32;

extern s32 D_801D06AC;
extern void resource_free(s32 resource);

void func_00201D24(void)
{
    resource_free(D_801D06AC);
    D_801D06AC = 0;
}
