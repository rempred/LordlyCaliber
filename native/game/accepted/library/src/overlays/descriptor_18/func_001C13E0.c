typedef unsigned char u8;
typedef signed int s32;
typedef struct { s32 used; u8 b[0x58]; } ScriptRes;
extern ScriptRes D_8022A4E0[];
extern u8 *D_8022A650;
void func_001C0344(u8 *p);
void func_001BFF9C(ScriptRes *r);
void func_001C13E0(void) {
    u8 *n = D_8022A650;
    u8 *last = ((void *)0);
    s32 i;
    if (n != ((void *)0)) {
        while (*(u8 **)(n + 8) != ((void *)0)) {
            n = *(u8 **)(n + 8);
        }
        last = n;
    }
    if (last != ((void *)0)) {
        func_001C0344(last);
    }
    for (i = 0; i < 4; i++) {
        if (D_8022A4E0[i].used != 0) {
            func_001BFF9C(&D_8022A4E0[i]);
        }
    }
}
