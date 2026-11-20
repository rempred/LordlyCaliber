typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
extern s32 D_801B4894[];
void bcopy(const void *src, void *dst, s32 n);
void *func_00001330(s32 size);
void func_000fd524(u8 *o, s32 a, s32 b, s32 c) {
    u8 *img = (*(u8 * *)((u8 *)(o) + (4)));
    s32 size;
    u32 i;
    size = (*(u16 *)((u8 *)(img) + (4))) * (*(u16 *)((u8 *)(img) + (6))) * D_801B4894[img[3]];
    size /= 8;
    size += 8;
    for (i = 0; i < 2; i++) {
        ((u8 **)(o + 0x38))[i] = func_00001330(size);
        bcopy((*(u8 * *)((u8 *)(o) + (4))), ((u8 **)(o + 0x38))[i], 8);
    }
    (*(s32 *)((u8 *)(o) + (0x34))) = 0;
    (*(s32 *)((u8 *)(o) + (0x40))) = a;
    (*(s32 *)((u8 *)(o) + (0x44))) = b;
    (*(s32 *)((u8 *)(o) + (0x48))) = c;
    (*(u16 *)((u8 *)(o) + (0))) = 2;
}
