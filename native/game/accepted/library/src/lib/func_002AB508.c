typedef unsigned char u8;
typedef signed int s32;
extern u8 D_8022A980;
void D_80227B80(void);
extern u8 D_8022A980;
void func_002AB508(s32 f) {
    if (f == 0) {
        D_8022A980 = 0xD7;
    } else {
        D_8022A980 = 0;
        D_80227B80();
    }
}
