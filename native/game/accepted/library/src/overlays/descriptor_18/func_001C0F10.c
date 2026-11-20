typedef unsigned char u8;
typedef signed int s32;
typedef struct { s32 used; u8 b[0x58]; } ScriptRes;
extern ScriptRes D_8022A4E0[];
extern u8 *D_8022A650;
void func_000016C4(void *p);
void func_001C0F10(s32 i) {
    u8 *r;
    if (i >= 4) {
        for (;;) {
        }
    }
    r = (u8 *)&D_8022A4E0[i];
    if (*(u8 **)(r + 4) != ((void *)0)) {
        *(u8 **)(*(u8 **)(r + 4) + 8) = *(u8 **)(r + 8);
    } else {
        D_8022A650 = *(u8 **)(r + 8);
    }
    if (*(u8 **)(r + 8) != ((void *)0)) {
        *(u8 **)(*(u8 **)(r + 8) + 4) = *(u8 **)(r + 4);
    }
    if (*(void **)(r + 0x34) != ((void *)0)) {
        func_000016C4(*(void **)(r + 0x34));
        *(void **)(r + 0x34) = ((void *)0);
    }
    if (*(void **)(r + 0x4C) != ((void *)0)) {
        func_000016C4(*(void **)(r + 0x4C));
        *(void **)(r + 0x4C) = ((void *)0);
    }
    *(s32 *)r = 0;
}
