typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u8 D_801939CC;
extern u16 D_801F1060[];
void func_00131404(void) {
    s32 i;
    for (i = 0; i < 6; i++) {
        if (D_801F1060[i] == 0 || --D_801F1060[i] == 0) {
            D_801939CC |= 1 << i;
        } else {
            D_801939CC &= ~(1 << i);
        }
    }
}
