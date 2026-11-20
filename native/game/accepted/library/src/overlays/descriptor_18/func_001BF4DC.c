typedef unsigned short u16;
extern u16 D_8022A4D8;
extern u16 D_8022A4DA;
extern u16 D_8022A4DC;
extern u16 D_8022A4DE;
extern u16 D_800E79B0;
extern u16 D_800E9BE4;
void func_001BF4DC(void) {
    u16 pa = D_8022A4DC;
    u16 a = D_800E79B0;
    u16 pb = D_8022A4DE;
    u16 b = D_800E9BE4;
    D_8022A4D8 = a & ~pa;
    D_8022A4DC = a;
    D_8022A4DA = b & ~pb;
    D_8022A4DE = b;
}
