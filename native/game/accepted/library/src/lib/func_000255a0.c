typedef struct ViContextView {
    unsigned int field_00;
    void *framebuffer;
} ViContextView;
extern ViContextView *D_800ABBD0;
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);

void *func_000255a0(void)
{
    unsigned int mask = __osDisableInt();
    void *framebuffer = D_800ABBD0->framebuffer;
    __osRestoreInt(mask);
    return framebuffer;
}
