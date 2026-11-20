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
extern u8 D_8022A821;
s32 func_001BBF18(ScriptCtx *ctx) {
    switch (ctx->pc[3]) {
        case 0:
            D_8022A821 = 2;
            break;
        case 1:
            D_8022A821 = 1;
            break;
    }
    ctx->pc += 4;
    return 0;
}
