typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_801CE8BC;
extern u8 *D_801CE8C0;
extern u8 D_8022A7F0;
void *func_00001330(s32 size);
void bzero(void *p, s32 n);
void D_801BE71C(void);
void D_801C1D90(void);
void D_801C1A94(void);
void D_801C2310(void);
void D_801AECA4(u8 a);
void func_002B8610(void) {
    u8 *p;
    D_801BE71C();
    p = func_00001330(0x6094);
    D_801CE8BC = p;
    bzero(p, 0x6094);
    D_801CE8C0 = D_801CE8BC + 0x57E0;
    D_801C1D90();
    D_801C1A94();
    D_801C2310();
    D_801AECA4(D_8022A7F0);
}
