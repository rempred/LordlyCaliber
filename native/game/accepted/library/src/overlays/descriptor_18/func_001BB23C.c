typedef unsigned char u8;
typedef signed short s16;
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
s32 func_001BB23C(ScriptCtx *ctx) {
    s16 n;
    u16 *p;
    u8 *start;
    if (ctx->loopDepth == 0) {
        for (;;) {
        }
    }
    p = &ctx->loopCount[ctx->loopDepth - 1];
    start = ctx->stack[ctx->depth - 1];
    n = ++*p;
    if (*(s16 *)(start + 2) < 0 || n < *(s16 *)(start + 2)) {
        ctx->pc = start + 4;
    } else {
        if (ctx->depth == 0) {
            for (;;) {
            }
        }
        ctx->depth--;
        ctx->loopDepth--;
        ctx->pc += 4;
    }
    return 0;
}
