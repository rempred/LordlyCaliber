extern void *volatile D_8022A950;
extern void *volatile D_8022A954;
extern void *volatile D_8022A958;
extern void *volatile D_8022A95C;

extern void resource_free(void *resource);

void func_0028422C(void)
{
    void *saved0;
    void *saved1;

    resource_free(D_8022A958);
    saved0 = D_8022A954;
    saved1 = D_8022A95C;
    D_8022A958 = 0;
    D_8022A950 = 0;
    D_8022A95C = 0;
    D_8022A950 = saved0;
    D_8022A958 = saved1;
}
