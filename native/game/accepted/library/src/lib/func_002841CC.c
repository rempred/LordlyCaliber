extern void *D_8022A958;

extern void resource_free(void *resource);
extern void func_00283FA8(void *arg0, int arg1);

void func_002841CC(void *arg0)
{
    resource_free(D_8022A958);
    D_8022A958 = 0;
    func_00283FA8(arg0, 0);
}
