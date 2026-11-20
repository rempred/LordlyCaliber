typedef signed char s8;
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
extern s8 D_800E9C0A[];
s32 func_001BBECC(ScriptCtx *ctx) {
    u8 *pc = ctx->pc;
    s32 v = *(s16 *)(pc + 2);
    switch (pc[1]) {
        case 0:
            D_800E9C0A[6] = v;
            break;
        case 1:
            D_800E9C0A[9] = v;
            break;
    }
    ctx->pc += 4;
    return 0;
}
