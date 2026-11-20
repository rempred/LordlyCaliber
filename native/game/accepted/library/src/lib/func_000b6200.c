typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
extern u8 *D_80196AF8;
void func_000b6200(void) {
    s32 pad;
    u8 *st = D_80196AF8;
    s16 v = (*(s16 *)((u8 *)(st) + (0x122)));
    switch (v) {
        case 0:
            (*(u16 *)((u8 *)(st) + (0x12E))) = 10;
            (*(u16 *)((u8 *)(st) + (0x122)))++;
            break;
        case 1:
            if ((*(u16 *)((u8 *)(st) + (0x12E))) == 0) {
                (*(u16 *)((u8 *)(st) + (0x122))) = v + 1;
            }
            break;
        case 2:
        case 99:
            (*(u16 *)((u8 *)(st) + (0x120))) = 999;
            break;
    }
}
