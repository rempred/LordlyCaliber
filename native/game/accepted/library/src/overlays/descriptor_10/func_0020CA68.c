typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
u8 *func_0020F2A8(s32 a, s32 x, s32 y, s32 z);
void func_001F7148();
void func_001F89B4();
extern s32 D_801CE8FC;
typedef struct Fx20B {
    void (*fn)();
    u8 unk04[0x10];
    u8 b14;
    u8 unk15[0x1F];
    s16 h34;
    u8 unk36[0xA];
    u8 *owner;
    s32 w44;
    u8 unk48[0x48];
    struct Fx20B *head;
} Fx20B;
void func_0020CA68(u8 *o, s32 k, s32 dx, s32 dy, s32 dz) {
    Fx20B *a;
    Fx20B *b;
    a = (Fx20B *)func_0020F2A8((*(s32 *)((u8 *)(o) + (0x50))), (*(s32 *)((u8 *)(o) + (0x5C))) + dx, (*(s32 *)((u8 *)(o) + (0x60))) + dy,
                                          (*(s32 *)((u8 *)(o) + (0x64))) + dz);
    a->b14 = 0;
    ((Fx20B **)o)[k] = a;
    b = (Fx20B *)func_0020F2A8((*(s32 *)((u8 *)(o) + (0x50))), (*(s32 *)((u8 *)(o) + (0x5C))) + dx, (*(s32 *)((u8 *)(o) + (0x60))) + dy,
                                          (*(s32 *)((u8 *)(o) + (0x64))) + dz);
    b->b14 = 0;
    ((Fx20B **)o)[k + 3] = b;
    b->owner = o;
    a->fn = func_001F7148;
    a->owner = o;
    b->fn = func_001F89B4;
    a->h34 = 0xFF;
    b->h34 = D_801CE8FC;
    a->w44 = b->w44 = (*(s32 *)((u8 *)(o) + (0x50)));
    b->head = a;
    a->head = a;
}
