typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk00[0x30];
    s32 w30;
    s32 w34;
    s32 w38;
    s32 w3C;
} Task271;
extern s32 D_80220F68;
Task271 *func_00262AB0(void *fn);
s32 func_0027267C(u16 msg, u8 *o);
void func_00271E30(s32 a) {
    D_80220F68 = a;
    func_00262AB0(func_0027267C);
}
s32 func_0027267C(u16 msg, u8 *o);
