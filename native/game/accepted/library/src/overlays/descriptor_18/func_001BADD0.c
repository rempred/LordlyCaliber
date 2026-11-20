typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_8022A234;
extern u8 D_801936E0;
extern u8 D_80196AEC;
void func_000016C4(void *p);
void func_001C2A40(void);
void func_001C1FF4(void);
void func_001C1488(void);
void func_001BDF2C(void);
void func_001BDCF8(void);
void D_80079618(s32 handle, s32 flags);
void func_001BADD0(void) {
    if (D_801936E0) {
        func_000016C4(D_8022A234);
        func_001C2A40();
        func_001C1FF4();
        func_001C1488();
        func_001BDF2C();
        func_001BDCF8();
        D_80196AEC &= 0x7F;
        D_80079618(0x21C3C38, -0x11);
    }
}
