typedef unsigned char u8;
typedef signed int s32;
extern void *D_8018FC10;
extern u8 D_8018FC44[];
extern u8 D_80243DB0[];
void func_0004ebcc(s32 flags);
void func_00001314(void);
void resource_arena_init(void *start, s32 size);
void D_8009DAEC(void);
void *func_0004e3bc(void) {
    D_8018FC10 = ((void *)0);
    func_0004ebcc(1);
    func_00001314();
    resource_arena_init(D_80243DB0, (u8 *)0x80400000 - D_80243DB0);
    D_8009DAEC();
    return D_8018FC44;
}
void func_0004ebcc(s32 flags);
