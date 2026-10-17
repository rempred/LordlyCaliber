typedef struct ViContextView {
    unsigned short state;
    unsigned short retraceCount;
    void *framebuffer;
    void *mode;
    unsigned int control;
    void *queue;
    void *message;
} ViContextView;
extern ViContextView *D_800ABBD4;
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);

void func_00025960(void *queue, void *message, unsigned int retraceCount)
{
    unsigned int mask = __osDisableInt();
    D_800ABBD4->queue = queue;
    D_800ABBD4->message = message;
    D_800ABBD4->retraceCount = retraceCount;
    __osRestoreInt(mask);
}
