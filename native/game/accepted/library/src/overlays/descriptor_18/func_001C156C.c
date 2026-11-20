typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct { s32 used; u8 b[0x58]; } ScriptRes;
extern ScriptRes D_8022A4E0[];
void bzero(void *p, s32 size);
extern u16 D_8022A4D8;
extern u16 D_8022A4DA;
extern u16 D_8022A4DC;
extern u16 D_8022A4DE;
extern u8 *D_8022A650;
extern s32 D_8022A838;
void func_001C156C(void) {
    bzero(D_8022A4E0, 0x170);
    D_8022A650 = 0;
    D_8022A4DE = 0;
    D_8022A4DC = 0;
    D_8022A4DA = 0;
    D_8022A4D8 = 0;
    D_8022A838 = -1;
}
