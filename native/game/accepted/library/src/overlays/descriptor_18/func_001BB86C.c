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
ScriptCtx *func_001BAAF0(u8 *base, s32 off, s32 idx);
ScriptCtx *func_001BAAF0(u8 *base, s32 off, s32 idx);
s32 func_001BB86C(ScriptCtx *ctx) {
    u8 *pc = ctx->pc;
    func_001BAAF0(*(u8 **)(pc + 4), *(s32 *)(pc + 8), *(u16 *)(pc + 2));
    ctx->pc += 12;
    return 0;
}
