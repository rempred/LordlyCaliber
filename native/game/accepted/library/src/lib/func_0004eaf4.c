typedef unsigned char u8;
typedef unsigned short u16;
extern struct { u8 pad[4]; u16 state; } *D_800C4BBC;
extern void *D_8018FC10;
extern u8 *D_8018FDC0;
void D_80227348(void);
void func_00019e10(void);
void D_8022F264(void);
void func_000016C4(void *p);
void func_0004eaf4(void) {
    D_80227348();
    func_00019e10();
    D_8022F264();
    D_8018FDC0[0xD2] = 0;
    if (D_800C4BBC != ((void *)0) && D_800C4BBC->state == 13) {
        D_800C4BBC->state = 2;
    }
    if (D_8018FC10 != ((void *)0)) {
        func_000016C4(D_8018FC10);
    }
}
