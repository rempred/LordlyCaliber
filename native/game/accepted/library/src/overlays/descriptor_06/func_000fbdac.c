typedef unsigned char u8;
typedef unsigned int u32;
void func_00100038(void **p);
void func_000fbdac(u8 *p) {
    u32 i;
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
}
void func_00100038(void **p);
