typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk0[0xC];
    void *data;
} Res2A3;
typedef struct {
    u8 unk0000[0x1CA4];
    Res2A3 *res;
} Battle2A3;
extern Battle2A3 *D_8022A974;
void func_002A4680(void);
s32 func_002a3fa0(void) {
    u8 *e = (u8 *)D_8022A974->res;
    if (e == ((void *)0)) {
        return 0;
    }
    if (e[0] < 2) {
        return 0;
    }
    switch (e[0]) {
        case 2:
        case 3:
            if (++*(u16 *)(e + 2) == *(u16 *)(e + 4)) {
                func_002A4680();
            }
            break;
        case 4:
            if (*(u16 *)(e + 2) != *(u16 *)(e + 4)) {
                (*(u16 *)(e + 2))++;
            }
            break;
    }
    return 1;
}
void func_002A4680(void);
