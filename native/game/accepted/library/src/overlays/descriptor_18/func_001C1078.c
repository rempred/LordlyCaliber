typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct { s32 used; u8 b[0x58]; } ScriptRes;
extern ScriptRes D_8022A4E0[];
void *D_8017F334(u16 id);
extern u8 *D_8022A650;
void func_000016C4(void *p);
extern inline void ScriptResFree(s32 i) {
    ScriptRes *e;
    if (i >= 4) {
        for (;;);
    }
    e = &D_8022A4E0[i];
    if ((*(u8 * *)((u8 *)(e) + (4))) != ((void *)0)) {
        (*(u8 * *)((u8 *)((*(u8 * *)((u8 *)(e) + (4)))) + (8))) = (*(u8 * *)((u8 *)(e) + (8)));
    } else {
        D_8022A650 = (*(u8 * *)((u8 *)(e) + (8)));
    }
    if ((*(u8 * *)((u8 *)(e) + (8))) != ((void *)0)) {
        (*(u8 * *)((u8 *)((*(u8 * *)((u8 *)(e) + (8)))) + (4))) = (*(u8 * *)((u8 *)(e) + (4)));
    }
    if ((*(void * *)((u8 *)(e) + (0x34))) != ((void *)0)) {
        func_000016C4((*(void * *)((u8 *)(e) + (0x34))));
        (*(void * *)((u8 *)(e) + (0x34))) = ((void *)0);
    }
    if ((*(void * *)((u8 *)(e) + (0x4C))) != ((void *)0)) {
        func_000016C4((*(void * *)((u8 *)(e) + (0x4C))));
        (*(void * *)((u8 *)(e) + (0x4C))) = ((void *)0);
    }
    e->used = 0;
}
s32 func_001C1078(s32 i) {
    void *o = D_8017F334(i | 0x5200);
    if (o == ((void *)0)) {
        ScriptResFree(i);
    }
    return o == ((void *)0);
}
