typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
extern u8 *D_801B3E20;
void func_000fd23c(u8 *p);
void func_000fd23c(u8 *p);
s32 func_00100328(void) {
    u8 *p = D_801B3E20;
    func_000fd23c(p + 0x1E4);
    (*(s16 *)((u8 *)(p) + (0x1E4))) = 1;
    (*(s32 *)((u8 *)(p) + (0x1F8))) = 0;
    (*(s32 *)((u8 *)(p) + (0x1EC))) = 0;
    (*(s32 *)((u8 *)(p) + (0x1F4))) = 0;
    (*(s32 *)((u8 *)(p) + (0x1E8))) = 0;
    (*(f32 *)((u8 *)(p) + (0x1F0))) = 15.0f;
    return 1;
}
