typedef struct ViModeView {
    unsigned int field_00;
    unsigned int control;
} ViModeView;
typedef struct ViContextView {
    unsigned short state;
    unsigned short retraceCount;
    void *framebuffer;
    ViModeView *mode;
    unsigned int control;
} ViContextView;
extern ViContextView *D_800ABBD4;
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);

void func_000259c0(ViModeView *mode)
{
    unsigned int mask = __osDisableInt();
    D_800ABBD4->mode = mode;
    D_800ABBD4->state = 1;
    D_800ABBD4->control = D_800ABBD4->mode->control;
    __osRestoreInt(mask);
}
