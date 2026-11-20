typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
extern u8 *D_8022A974;
void *func_00001330(s32 size);
extern s16 D_8022A994;
extern u8 D_8018FC19;
void bzero(void *p, s32 n);
void D_801C22D0(s32 a);
void D_801C45E0(void);
void D_8020B098(void);
void D_801CBD48(void);
void D_801BD1F4(void);
void D_801707F4(void);
void D_80227FD8(s16 a, s32 b);
void D_8022D120(void);
void D_80227010(void);
void func_002A90EC(void);
void func_002A8D20(void) {
    D_8022A974 = func_00001330(0x1CB8);
    bzero(D_8022A974, 0x1CB8);
    D_801C22D0(2);
    D_801C45E0();
    D_8020B098();
    D_801CBD48();
    D_801BD1F4();
    D_801707F4();
    D_80227FD8(D_8022A994, 0);
    if (D_8018FC19 == 2) {
        func_002A90EC();
    }
    D_8022D120();
    D_80227010();
}
void func_002A90EC(void);
