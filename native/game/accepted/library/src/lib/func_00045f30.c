typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
extern u8 D_80195560[][0x34];
extern u16 D_801953F4[];
extern u8 D_801971F0[][0x19];
extern u8 D_801969B8[][0xB];
void func_00045f30(void) {
    s16 i;
    for (i = 1; i < 99; i++) {
        bzero(D_80195560[i], 0x34);
    }
    for (i = 0; i < 120; i++) {
        D_801953F4[i] = 0;
    }
    for (i = 30; i < 50; i++) {
        bzero(D_801971F0[i], 0x19);
    }
    for (i = 0; i < 10; i++) {
        if (D_801969B8[i][1] & 0x80) {
            bzero(D_801969B8[i], 0xB);
        }
    }
}
