typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct Node {
    struct Node *prev;
    struct Node *next;
    u8 unk08[0xE];
    u16 id;
} Node;
Node *func_00051f74(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h);
Node *func_00051eb0(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h, s16 x2, s16 y2, s16 w2, s16 h2) {
    Node *n = func_00051f74(kind, b, fn, x, y, w, h);
    (*(s16 *)((u8 *)(n) + (0x18))) = 0;
    (*(s32 *)((u8 *)(n) + (0x3C))) = x2;
    (*(s32 *)((u8 *)(n) + (0x40))) = y2;
    (*(s32 *)((u8 *)(n) + (0x44))) = w2;
    (*(s32 *)((u8 *)(n) + (0x48))) = h2;
    (*(s16 *)((u8 *)(n) + (0x28))) = 1;
    (*(u16 *)((u8 *)(n) + (0x26))) = 0x8000;
    return n;
}
