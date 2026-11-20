typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
void *func_000016C4(void *p);
extern s16 *D_8022A97C;
void *func_00001330(s32 size);
s32 D_8009DAF4(s32 id);
void D_8009DBB8(void *dst, s32 id);
u32 func_0000ABE0(void *p);
void func_0000A510(void *dst, void *src);
void func_002A21E0(void);
void func_0029C950(void) {
    u8 *tmp;
    if (D_8022A97C != ((void *)0)) {
        func_000016C4(D_8022A97C);
    }
    tmp = func_00001330(D_8009DAF4(0x109EEB0));
    D_8009DBB8(tmp, 0x109EEB0);
    D_8022A97C = func_00001330(func_0000ABE0(tmp));
    func_0000A510(D_8022A97C, tmp);
    func_000016C4(tmp);
    func_002A21E0();
}
