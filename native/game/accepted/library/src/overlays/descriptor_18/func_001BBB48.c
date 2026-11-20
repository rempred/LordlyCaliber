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
u8 *func_001BBFFC(u16 id);
void func_001BD378(u8 *o, u16 a, s32 b);
s32 func_001BBB48(ScriptCtx *ctx) {
    u8 *o = func_001BBFFC(ctx->pc[1]);
    if (*o != 1) {
        for (;;) {
        }
    }
    func_001BD378(o, *(u16 *)(ctx->pc + 2), 0);
    ctx->pc += 4;
    return 0;
}
