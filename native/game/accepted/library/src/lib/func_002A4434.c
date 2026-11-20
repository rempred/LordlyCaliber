typedef unsigned char u8;
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
extern u8 D_8018FC19;
extern s32 D_80197774;
extern u8 D_8022A960;
void D_801C22E8(s32 a);
void D_801C45E0(void);
void D_801B7E9C(void);
void D_8023AA84(s32 a);
void D_8023BF30(u8 a, s32 b);
void func_002A2DB0(void);
void func_0029B60C(void);
void func_002A4434(void) {
    u8 i;
    D_801C22E8(1);
    D_801C45E0();
    if (D_8018FC19 == 2) {
        D_80197774 = 0;
        D_801B7E9C();
    } else {
        for (i = 0; i < 20; i++) {
            D_8023AA84(((s32 *)((u8 *)D_8022A974 + 0x1C04))[i]);
            ((s32 *)((u8 *)D_8022A974 + 0x1C04))[i] = 0;
        }
        D_8023BF30(D_8022A960, 0);
    }
    func_002A2DB0();
    func_0029B60C();
}
