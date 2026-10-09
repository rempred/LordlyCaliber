typedef signed int s32;

extern s32 D_800AF0B0;
extern void func_00049aa0(s32 resource);

void boot_resource_global_handle_release(void)
{
    func_00049aa0(D_800AF0B0);
    D_800AF0B0 = 0;
}
