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
s32 func_001BB154(ScriptCtx *ctx) {
    s32 r = 0;
    s16 n;
    if (ctx->state == 0) {
        ctx->cond = 0;
        ctx->state = 1;
    }
    n = *(s16 *)(ctx->pc + 2);
    if (n == -1) {
        r = 1;
    } else if ((s16)ctx->cond < n) {
        ctx->cond++;
        r = 1;
    } else {
        ctx->state = 0;
        ctx->pc += 4;
    }
    return r;
}
