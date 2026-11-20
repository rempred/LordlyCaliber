typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
extern u8 D_8022A820;
extern u8 D_8022A834;
extern s16 D_8022A832;
u8 *D_8017C074(u16 a, u8 b, void *fn, s16 c, s16 d, s16 e, s16 f);
void func_001C2CE4(void);
void func_001C2C54(void) {
    D_8022A820 = 0;
    D_8022A834 = 8;
    D_8022A832 = 0;
    D_8017C074(0x5501, 0x13, func_001C2CE4, 0, 0, 0, 0);
}
void func_001C2CE4(void);
