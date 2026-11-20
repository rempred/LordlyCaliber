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
extern u16 D_8022A256;
s32 func_001BB838(ScriptCtx *ctx) {
    u16 bits = *(u16 *)(ctx->pc + 2);
    ctx->pc += 4;
    D_8022A256 &= ~bits;
    return 0;
}
