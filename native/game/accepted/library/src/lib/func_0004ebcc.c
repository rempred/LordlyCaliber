typedef signed int s32;
void boot_resource_global_handle_release(void);
void func_0004f1bc(void);
void func_00051e10(void);
void func_00003798(void);
void func_0004ebcc(s32 flags);
void func_0004ebcc(s32 flags) {
    boot_resource_global_handle_release();
    func_0004f1bc();
    func_00051e10();
    if (flags & 1) {
        func_00003798();
    }
}
