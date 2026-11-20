typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk00;
    u8 state;
    u8 unk02[2];
    u8 *base;
    u8 *volatile pc;
    u16 depth;
    u8 unk0E[2];
    u8 *stack[4];
    u16 loopDepth;
    u16 loopCount[4];
    u8 unk2A[2];
    u16 cond;
} ScriptCtx;
u8 *func_001BBFB0(u16 id);
void func_001BD378(u8 *o, u16 a, s32 b);
s32 func_001BB95C(ScriptCtx *c) {
    u8 *o = func_001BBFB0(c->pc[1]);
    o[0] = 1;
    (*(u16 *)((u8 *)(o) + (2))) = (*(u16 *)((u8 *)(c->pc) + (4)));
    (*(u16 *)((u8 *)(o) + (4))) = (*(u16 *)((u8 *)(c->pc) + (6)));
    (*(u16 *)((u8 *)(o) + (0x10))) = 0;
    (*(u16 *)((u8 *)(o) + (0xE))) = 0;
    (*(u16 *)((u8 *)(o) + (6))) = (*(u16 *)((u8 *)(o) + (0xA))) = (*(u16 *)((u8 *)(o) + (2)));
    (*(u16 *)((u8 *)(o) + (8))) = (*(u16 *)((u8 *)(o) + (0xC))) = (*(u16 *)((u8 *)(o) + (4)));
    func_001BD378(o, (*(u16 *)((u8 *)(c->pc) + (2))), 1);
    c->pc += 8;
    return 0;
}
