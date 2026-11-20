typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    s32 id;
    u16 flags;
    u8 unk06[5];
    u8 bB;
} MapEntry;
extern u16 D_801FDA32;
extern MapEntry **D_801FDA34;
s32 func_00147F1C(s32 id) {
    s32 pad;
    s32 i;
    for (i = 0; i < D_801FDA32; i++) {
        if (D_801FDA34[i] != ((void *)0) && D_801FDA34[i]->id == id) {
            break;
        }
    }
    if (i >= D_801FDA32) {
        for (;;) {
        }
    }
    return (*(u32 *)&D_801FDA34[i]->flags & 0xC000000) != 0;
}
