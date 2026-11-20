typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    s32 id;
    u16 flags;
    u8 unk06[5];
    u8 bB;
} MapEntry;
extern u16 D_801FDA32;
extern MapEntry **D_801FDA34;
inline MapEntry *func_00147B80(s32 id) {
    s32 pad;
    s32 i;
    MapEntry *r = ((void *)0);
    for (i = 0; i < D_801FDA32; i++) {
        if (D_801FDA34[i] != ((void *)0) && D_801FDA34[i]->id == id) {
            r = D_801FDA34[i];
            break;
        }
    }
    return r;
}
