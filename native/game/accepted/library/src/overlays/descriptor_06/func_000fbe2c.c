typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef struct {
    s16 h0;
    u8 unk02[2];
    s32 w4;
    u8 unk08[4];
    s32 wC;
    u8 unk10[0x74];
    s32 x84;
    u8 x88;
    u8 x89;
    u8 x8A;
    u8 x8B;
} Widget3390;
void func_000fbe2c(Widget3390 *p);
void func_000016C4(void *p);
void func_00100038(void **p);
void func_000016C4(void *p);
void func_000fbe2c(Widget3390 *w) {
    u8 *p = (u8 *)w;
    u32 i;
    u32 j;
    (*(s32 *)((u8 *)(p) + (0x194))) = 0;
    for (i = 0; i < 3; i++) {
        if ((*(void * *)((u8 *)(p) + (0x524 + i * 4))) != ((void *)0)) {
            func_00100038((*(void * *)((u8 *)(p) + (0x524 + i * 4))));
        }
    }
    for (i = 0; i < 3; i++) {
        if ((*(void * *)((u8 *)(p) + (0x530 + i * 4))) != ((void *)0)) {
            func_00100038((*(void * *)((u8 *)(p) + (0x530 + i * 4))));
        }
    }
    for (j = 0; j < 18; j++) {
        func_000016C4((*(void * *)((u8 *)(p) + (0x19C + j * 4))));
    }
}
void func_00100038(void **p);
