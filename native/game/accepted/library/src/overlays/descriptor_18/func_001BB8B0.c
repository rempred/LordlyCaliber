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
extern u8 *D_8022A234;
void bzero(void *p, s32 size);
void bzero(void *p, s32 n);
s32 func_001BB8B0(ScriptCtx *ctx) {
    u8 *p = D_8022A234 + *(u16 *)(ctx->pc + 2) * 0x38;
    if (p[0] == 0) {
        for (;;) {
        }
    }
    bzero(p, 0x38);
    ctx->pc += 4;
    return 0;
}
