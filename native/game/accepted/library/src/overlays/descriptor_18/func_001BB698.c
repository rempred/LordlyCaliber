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
s32 func_001BB698(ScriptCtx *ctx) {
    s32 r = 0;
    if (D_8022A82C == *(s32 *)(ctx->pc + 4)) {
        D_8022A82C = 0;
        ctx->pc += 8;
    } else {
        r = 1;
    }
    return r;
}
