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
extern s32 D_8022A82C;
s32 func_001BB674(ScriptCtx *ctx) {
    s32 v = *(s32 *)(ctx->pc + 4);
    ctx->pc = ctx->pc + 8;
    D_8022A82C = v;
    return 0;
}
