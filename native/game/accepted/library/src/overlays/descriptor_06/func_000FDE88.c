typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
s32 rand(void);
void func_000FDE88(u8 *p, s32 a, s32 b) {
    s32 pad[4];
    (*(s16 *)((u8 *)(p) + (0x14))) = 0xFF;
    (*(s16 *)((u8 *)(p) + (0x12))) = 0xFF;
    (*(s16 *)((u8 *)(p) + (0xC))) = 0xFF;
    (*(u16 *)((u8 *)(p) + (0))) = 1;
    (*(s32 *)((u8 *)(p) + (4))) = a;
    (*(s32 *)((u8 *)(p) + (8))) = b;
    (*(s16 *)((u8 *)(p) + (0x10))) = 0;
    (*(s16 *)((u8 *)(p) + (0xE))) = 0;
    (*(s16 *)((u8 *)(p) + (0x18))) = 10;
    rand();
    rand();
    (*(s16 *)((u8 *)(p) + (0x16))) = rand() & 0xF;
    (*(s16 *)((u8 *)(p) + (0x1A))) = 0;
}
