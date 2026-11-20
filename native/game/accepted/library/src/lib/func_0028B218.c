typedef unsigned char u8;
typedef signed int s32;
extern u8 D_8023DE3C;
extern u8 D_800E7A0C;
extern u8 D_8018FC00[];
void func_00019e10(void);
void func_00019eb0(void *p, s32 n);
void D_8016CD5C(s32 a);
void func_0028B218(void) {
    func_00019e10();
    func_00019eb0(D_8018FC00, 2);
    func_00019e10();
    D_8016CD5C(1);
    D_800E7A0C = 0;
    D_8023DE3C = 0;
}
