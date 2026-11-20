typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    s32 unk00;
    s32 count;
    u8 pad08[4];
    s32 flags;
    void *ptrbank;
    u32 unk14;
    struct {
        u32 ptr;
        u32 unk4;
    } entries[1];
} MusFxBank;
extern void *D_800B1828;
extern MusFxBank *D_800B1838;
extern MusFxBank *D_800B183C;
s32 func_00013f24(MusFxBank *bank, s32 number, s32 volume, s32 pan, s32 priority);
s32 func_00011b70(s32 number) {
    MusFxBank *bank = D_800B183C;
    s32 ret;
    if (bank != ((void *)0)) {
        D_800B183C = ((void *)0);
    } else {
        bank = D_800B1838;
        if (bank == ((void *)0)) {
            D_800B1828 = ((void *)0);
            return 0;
        }
    }
    if (D_800B1828 == ((void *)0)) {
        D_800B1828 = bank->ptrbank;
    }
    ret = func_00013f24(bank, number, 0x80, 0x80, -1);
    D_800B1828 = ((void *)0);
    return ret;
}
s32 func_00013f24(MusFxBank *bank, s32 number, s32 volume, s32 pan, s32 priority);
