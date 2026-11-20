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
s32 func_001BB1C4(ScriptCtx *ctx) {
    u8 *ret;
    if (ctx->loopDepth >= 4) {
        for (;;) {
        }
    }
    ctx->loopCount[ctx->loopDepth++] = 0;
    ret = ctx->pc;
    if (ctx->depth >= 4) {
        for (;;) {
        }
    }
    ctx->stack[ctx->depth++] = ret;
    ctx->pc += 4;
    return 0;
}
