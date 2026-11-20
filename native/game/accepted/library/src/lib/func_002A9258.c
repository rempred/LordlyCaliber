typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
extern u8 *D_8022A974;
void func_002A9258(void) {
    s32 pad;
    u8 *o = (*(u8 * *)((u8 *)(D_8022A974) + (0x19F0)));
    if (o != ((void *)0) && (*(s16 *)((u8 *)(o) + (0x48))) != 0) {
        (*(s16 *)((u8 *)(o) + (0x48)))--;
        (*(f32 *)((u8 *)(o) + (0x30))) += (*(f32 *)((u8 *)(o) + (0xC)));
        (*(f32 *)((u8 *)(o) + (0x34))) += (*(f32 *)((u8 *)(o) + (0x10)));
        (*(f32 *)((u8 *)(o) + (0x24))) += (*(f32 *)((u8 *)(o) + (0x0)));
        (*(f32 *)((u8 *)(o) + (0x28))) += (*(f32 *)((u8 *)(o) + (0x4)));
    }
}
