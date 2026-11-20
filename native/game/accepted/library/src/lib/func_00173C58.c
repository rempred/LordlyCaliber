typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u8 D_80214E84;
extern s32 D_801F0DE0;
extern s32 D_801F3658;
extern u8 *D_801F0CB0[];
extern u8 D_801F0FDF;
extern u8 D_801F0FE0;
extern u8 D_801F0FE1;
extern u8 D_80210219;
extern u8 D_801951CC[];
u8 D_801DADF0(u8 *u);
s32 D_801DD244(void);
s32 D_80186210(s32 a);
void func_00173C58(void) {
    u8 *u = D_801F0CB0[D_801F3658];
    u8 pad[16];
    if (D_80214E84 != 0) {
        return;
    }
    D_801F0DE0 = 4;
    D_801F0FE1 = D_801DADF0(u);
    D_801F0FDF = 5;
    D_801F0FE0 = 1;
    if ((D_801DD244() && !(*(u16 *)(D_801951CC + *(s32 *)(u + 0x74) * 36) & 6)) || D_80186210(30)) {
        D_801F0FDF = 0;
        D_801F0FE0 = 2;
    } else if (D_801DD244()) {
        D_801F0FE0 = 0;
    }
    D_80210219 = 0x11;
}
