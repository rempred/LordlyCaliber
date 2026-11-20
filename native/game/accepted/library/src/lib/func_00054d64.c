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
void func_000566f4(Node *n, s32 a);
void func_00052484();
void func_00054d64(s32 a, u16 kind, s16 x, s16 y, s16 w, s16 h) {
    Node *n = func_00051f74(kind | 0x100, 0, func_00052484, x, y, w, h);
    (*(s16 *)((u8 *)(n) + (0x28))) = 1;
    (*(s16 *)((u8 *)(n) + (0x18))) = 0;
    (*(s32 *)((u8 *)(n) + (0x3C))) = x;
    (*(s32 *)((u8 *)(n) + (0x40))) = y;
    (*(s32 *)((u8 *)(n) + (0x44))) = w;
    (*(s32 *)((u8 *)(n) + (0x48))) = h;
    (*(u16 *)((u8 *)(n) + (0x26))) = 0x8000;
    func_000566f4(n, a);
}
