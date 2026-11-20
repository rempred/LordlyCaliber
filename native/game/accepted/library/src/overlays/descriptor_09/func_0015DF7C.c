typedef unsigned char u8;
typedef signed int s32;
extern void *D_80213EE8;
extern u8 D_801F36A0;
extern s32 D_801F401C;
extern u8 D_80213EA4[];
void *D_8009DD38(s32 id);
void func_0015DF7C(void) {
    u8 f;
    if (D_80213EE8 == ((void *)0)) {
        D_80213EE8 = D_8009DD38(0x1DC326E);
    }
    f = D_801F36A0;
    if ((f & 1) && !(f & 4) && D_801F401C < 100) {
        s32 i;
        if (!(f & 2)) {
            i = 0x30;
        } else {
            i = 0x18;
        }
        D_80213EA4[i] = 1;
    }
}
