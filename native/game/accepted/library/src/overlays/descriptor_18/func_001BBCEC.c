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
u8 *func_001BBFFC(u16 id);
s32 func_001BBCEC(ScriptCtx *ctx) {
    u8 *o = func_001BBFFC(ctx->pc[1]);
    if (o[0] != 2) {
        for (;;) {
        }
    }
    o[0x16] = *(u16 *)(ctx->pc + 2);
    *(s16 *)(o + 6) = *(u16 *)(ctx->pc + 4);
    *(s16 *)(o + 8) = *(u16 *)(ctx->pc + 6);
    *(s16 *)(o + 0xA) = *(u16 *)(ctx->pc + 8);
    *(s16 *)(o + 0xC) = *(u16 *)(ctx->pc + 0xA);
    *(s16 *)(o + 0xE) = *(u16 *)(ctx->pc + 0xC);
    *(s16 *)(o + 0x10) = *(u16 *)(ctx->pc + 0xE);
    *(s16 *)(o + 0x12) = *(u16 *)(ctx->pc + 0x10);
    *(s16 *)(o + 0x14) = *(u16 *)(ctx->pc + 0x12);
    ctx->pc += 0x14;
    return 0;
}
