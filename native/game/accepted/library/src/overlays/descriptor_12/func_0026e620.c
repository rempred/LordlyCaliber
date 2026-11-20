typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk00[0x3C];
    s32 w3C;
    s32 w40;
    s32 w44;
    s32 w48;
    s32 w4C;
    s32 w50;
} Task26D;
extern s32 D_80220F68;
Task26D *func_00262AB0(void *fn);
s32 func_0026F12C(u16 msg, u8 *o);
void func_0026e620(s32 a) {
    D_80220F68 = a;
    func_00262AB0(func_0026F12C);
}
s32 func_0026F12C(u16 msg, u8 *o);
