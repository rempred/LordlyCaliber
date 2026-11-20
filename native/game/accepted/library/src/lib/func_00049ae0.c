typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
extern void *D_8018FDC0;
void func_00054d64(s32 a, u16 b, s16 c, s16 d, s16 e, s16 f);
void func_00049ae0(void *list, s32 a, u16 b, s16 c, s16 d, s16 e, s16 f) {
    void *old = D_8018FDC0;
    D_8018FDC0 = list;
    func_00054d64(a, b, c, d, e, f);
    D_8018FDC0 = old;
}
