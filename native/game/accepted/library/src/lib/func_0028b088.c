typedef unsigned char u8;
typedef signed int s32;
extern s32 D_800A8204[];
extern s32 D_800C4BB8;
extern u8 D_800E7A0C;
void func_00019e10(void);
void func_00019eb0(void *p, s32 n);
void D_8016CD5C(s32 a);
void func_0028b088(void) {
    func_00019e10();
    func_00019eb0(D_800A8204, 3);
    D_800C4BB8 = D_800A8204[0];
    func_00019e10();
    D_8016CD5C(0);
    D_800E7A0C = 1;
}
