typedef signed int s32;

extern s32 D_801F0CAC;
extern void resource_free(s32 resource);

void func_00113C60(void)
{
    resource_free(D_801F0CAC);
    D_801F0CAC = 0;
}
