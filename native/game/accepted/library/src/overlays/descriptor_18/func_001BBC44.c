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
s32 func_001BBC44(ScriptCtx *ctx) {
    u8 *o = func_001BBFFC(ctx->pc[1]);
    u8 *p;
    s16 x0;
    s16 x1;
    s16 y0;
    s16 y1;
    if (o[0] != 2) {
        for (;;) {
        }
    }
    o[0x16] = *(u16 *)(ctx->pc + 2);
    p = ctx->pc;
    x0 = *(u16 *)(p + 4);
    x1 = *(u16 *)(p + 6);
    y0 = *(u16 *)(p + 8);
    y1 = *(u16 *)(p + 0xA);
    *(s16 *)(o + 6) = -x0;
    *(s16 *)(o + 8) = -x1;
    *(s16 *)(o + 0xE) = -y0;
    *(s16 *)(o + 0xA) = x0;
    *(s16 *)(o + 0xC) = x1;
    *(s16 *)(o + 0x10) = -y1;
    *(s16 *)(o + 0x12) = y0;
    *(s16 *)(o + 0x14) = y1;
    ctx->pc += 0xC;
    return 0;
}
