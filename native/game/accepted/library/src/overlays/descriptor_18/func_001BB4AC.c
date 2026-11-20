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
void *D_8017F334(u16 id);
void D_8017C2FC(void *o);
void D_8017C384(void *o);
s32 func_001BB4AC(ScriptCtx *c) {
    void *o = D_8017F334(c->pc[3] | 0x5200);
    if (o == ((void *)0)) {
        for (;;) {
        }
    }
    if (c->pc[2] != 0) {
        D_8017C384(o);
    } else {
        D_8017C2FC(o);
    }
    c->pc += 4;
    return 0;
}
