extern void *D_8022A950;
extern void *D_8022A958;

extern void resource_free(void *resource);

void func_00284150(void)
{
    resource_free(D_8022A958);
    D_8022A958 = 0;
    D_8022A950 = 0;
}
