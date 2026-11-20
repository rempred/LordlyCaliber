typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
extern u8 D_80197810[][6];
extern u16 D_80196A30[];
void func_00045cfc(u8 id) {
    s16 i;
    for (i = 0; i < 121; i++) {
        u8 *e = D_80197810[i];
        if (e[0] == 0xFF) {
            e[0] = id;
            *(u16 *)(e + 2) = D_80196A30[0];
            e[4] = ((u8 *)D_80196A30)[2];
            e[5] = ((u8 *)D_80196A30)[3];
            return;
        }
    }
}
