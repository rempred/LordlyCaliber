typedef unsigned char u8;
typedef unsigned int u32;
extern u8 D_801BA710[];
void boot_resource_global_handle_release(void);
void D_801792BC(void);
void D_8017BF10(void);
void func_00003798(void);
void D_80179058(void);
void func_00001314(void);
void resource_arena_init(void *p, u32 size);
void D_8009DAEC(void);
void func_001CF960(void) {
    boot_resource_global_handle_release();
    D_801792BC();
    D_8017BF10();
    func_00003798();
    D_80179058();
    func_00001314();
    resource_arena_init(D_801BA710, 0x80400000 - (u32)D_801BA710);
    D_8009DAEC();
}
