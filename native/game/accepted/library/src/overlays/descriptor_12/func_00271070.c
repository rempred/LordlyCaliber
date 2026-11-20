typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern s32 D_80220F68;
s32 func_00271D18(u16 msg, u8 *o);
void *func_00262AB0(void *fn);
void func_00271070(s32 a) {
    D_80220F68 = a;
    func_00262AB0(func_00271D18);
}
s32 func_00271D18(u16 msg, u8 *o);
