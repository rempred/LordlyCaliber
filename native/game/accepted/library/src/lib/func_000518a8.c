typedef unsigned char u8;
typedef signed int s32;
extern s32 D_801977C8;
void func_00042714(u8 *p);
void D_801A101C(void);
void D_801B7FA0(u8 *p);
void D_8019B040(s32 a);
void func_000518a8(u8 *p) {
    func_00042714(p);
    switch (D_801977C8) {
        case 0:
            break;
        case 1:
            D_801A101C();
            break;
        case 2:
            D_801B7FA0((*(u8 * *)((u8 *)(p) + (0x10))) + 0x200);
            break;
        case 3:
            D_8019B040((*(s32 *)((u8 *)(p) + (0x10))));
            break;
    }
}
