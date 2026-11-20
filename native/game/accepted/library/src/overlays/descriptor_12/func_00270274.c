typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern s32 D_80220F68;
void *func_00262AB0(void *fn);
s32 func_002700B0(u16 msg, u8 *o);
void func_00270274(s32 a) {
    D_80220F68 = a;
    func_00262AB0(func_002700B0);
}
