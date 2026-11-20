typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u16 flags;
    u8 pref;
    u8 unk03[0x21];
} LocationRec;
extern LocationRec D_801951CC[];
extern u8 D_801936A9;
void func_0012FDFC(s32 loc) {
    s32 v;
    if (D_801951CC[loc].pref != 0xFF) {
        v = D_801936A9 - 2;
        if (v > 100) {
            v = 100;
        } else if (v < 0) {
            v = 0;
        }
        D_801936A9 = v;
    }
}
