typedef unsigned char u8;
typedef unsigned short u16;
extern u16 D_800E9C0A[];
extern u16 D_80190F30;
extern struct { u8 pad[4]; u16 state; } *D_800C4BBC;
extern u8 D_8018FC19;
extern void *D_8018FC10;
void D_80227348(void);
void func_000016C4(void *p);
void D_801B00B0();
extern void *D_80197AF0;
void D_801C58F0(void);
void func_0004ea5c(void) {
    func_000016C4(D_80197AF0);
    D_800E9C0A[1] = D_80190F30;
    D_80227348();
    if (D_8018FC19 != 0) {
        D_801B00B0();
    }
    func_0004ed30();
    if (D_800C4BBC != ((void *)0) && D_800C4BBC->state == 0xD) {
        D_800C4BBC->state = 2;
    }
    D_801C58F0();
    if (D_8018FC10 != ((void *)0)) {
        func_000016C4(D_8018FC10);
    }
}
void func_0004ed30(void);
