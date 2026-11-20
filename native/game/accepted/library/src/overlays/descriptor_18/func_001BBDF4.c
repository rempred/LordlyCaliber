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
extern u8 *D_8022A248;
extern s32 D_8022A24C;
extern volatile s32 D_8022A250;
void bzero(void *p, s32 size);
void bzero(void *p, s32 n);
extern s32 D_8022A244;
extern inline void func_80222CE4_clear(void) {
    if (D_8022A248 != ((void *)0) && D_8022A24C != 0) {
        bzero(D_8022A248, D_8022A24C);
    }
    D_8022A250 = 0;
}
s32 func_001BBDF4(ScriptCtx *ctx) {
    switch (ctx->pc[1]) {
        case 0: {
            s32 v;
            if (D_8022A248 != ((void *)0) && D_8022A24C != 0) {
                bzero(D_8022A248, D_8022A24C);
            }
            v = ((s32 *)D_8022A244)[*(u16 *)(ctx->pc + 2)];
            D_8022A250 = 0;
            D_8022A250 = v;
            break;
        }
        case 1:
            func_80222CE4_clear();
            break;
    }
    ctx->pc += 4;
    return 0;
}
