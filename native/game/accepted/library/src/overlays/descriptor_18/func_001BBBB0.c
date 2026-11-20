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
u8 *func_001BBFB0(u16 id);
s32 func_001BBBB0(ScriptCtx *ctx) {
    u8 *o = func_001BBFB0(*(u16 *)(ctx->pc + 2));
    o[0] = 2;
    *(u16 *)(o + 2) = *(u16 *)(ctx->pc + 4);
    *(u16 *)(o + 4) = *(u16 *)(ctx->pc + 6);
    *(s16 *)(o + 6) = -20;
    *(s16 *)(o + 8) = -20;
    *(s16 *)(o + 0xA) = 20;
    *(s16 *)(o + 0xC) = 20;
    *(s16 *)(o + 0xE) = -12;
    *(s16 *)(o + 0x10) = -12;
    *(s16 *)(o + 0x12) = 12;
    *(s16 *)(o + 0x14) = 12;
    o[0x16] = 16;
    ctx->pc += 8;
    return 0;
}
