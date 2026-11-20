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
extern u8 D_801936E0;
void bzero(void *p, s32 size);
extern s32 D_8022A230;
void bzero(void *p, s32 n);
ScriptCtx *func_001BAB84(u8 *base, s32 off) {
    ScriptCtx *ctx;
    s32 i;
    if (!D_801936E0) {
        return ((void *)0);
    }
    if (D_8022A234 == ((void *)0)) {
        for (;;) {
        }
    }
    for (i = 0; i < D_8022A230; i++) {
        if (D_8022A234[i * 0x38] == 0) {
            break;
        }
    }
    if (i < 0 || i >= D_8022A230) {
        for (;;) {
        }
    }
    ctx = (ScriptCtx *)(D_8022A234 + i * 0x38);
    bzero(ctx, 0x38);
    ctx->pc = base + off;
    ctx->base = base;
    ctx->unk00 = 1;
    return ctx;
}
