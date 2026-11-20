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
void func_001C1F98(s32 i);
void func_001C20F8(s32 i);
s32 func_001C2150(s32 i);
s32 func_001BB5CC(ScriptCtx *ctx) {
    u8 *pc = ctx->pc;
    u8 id = pc[3];
    s32 r = 1;
    switch (ctx->state) {
        case 0:
            if (pc[2] != 0) {
                func_001C20F8(id);
                ctx->state++;
            } else {
                r = 0;
            }
            break;
        case 1:
            r = func_001C2150(id) == 0;
            break;
    }
    if (!r) {
        func_001C1F98(id);
        ctx->state = 0;
        ctx->pc += 4;
    }
    return r;
}
