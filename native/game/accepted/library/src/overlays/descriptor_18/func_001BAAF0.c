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
ScriptCtx *func_001BAAF0(u8 *base, s32 off, s32 idx);
extern u8 *D_8022A234;
void bzero(void *p, s32 size);
extern s32 D_8022A230;
void bzero(void *p, s32 n);
ScriptCtx *func_001BAAF0(u8 *base, s32 off, s32 idx) {
    ScriptCtx *c;
    if (idx < 0 || idx >= D_8022A230) {
        for (;;) {
        }
    }
    c = (ScriptCtx *)(D_8022A234 + idx * 0x38);
    bzero(c, 0x38);
    c->pc = base + off;
    c->unk00 = 1;
    c->base = base;
    return c;
}
