extern void *D_8022A950;
extern void *D_8022A958;
extern void *D_8022A95C;

extern void resource_free(void *resource);

void func_00284184(void)
{
    resource_free(D_8022A958);
    resource_free(D_8022A95C);
    D_8022A958 = 0;
    D_8022A95C = 0;
    D_8022A950 = 0;
}
