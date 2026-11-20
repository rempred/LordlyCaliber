typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
void func_000fd26c(u8 *p) {
    s32 pad;
    if ((*(u16 *)((u8 *)(p) + (0))) != 0) {
        if ((*(s16 *)((u8 *)(p) + (0x1A))) < (*(s16 *)((u8 *)(p) + (0x1C)))) {
            (*(s16 *)((u8 *)(p) + (0x18))) = (*(s16 *)((u8 *)(p) + (0x1E))) + ((*(s16 *)((u8 *)(p) + (0x20))) - (*(s16 *)((u8 *)(p) + (0x1E)))) * (*(s16 *)((u8 *)(p) + (0x1A))) / (*(s16 *)((u8 *)(p) + (0x1C)));
            (*(s16 *)((u8 *)(p) + (0x1A)))++;
        } else {
            (*(s16 *)((u8 *)(p) + (0x18))) = (*(s16 *)((u8 *)(p) + (0x20)));
        }
    }
}
