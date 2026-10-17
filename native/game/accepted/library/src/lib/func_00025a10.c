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

void func_00025a10(unsigned int features)
{
    unsigned int mask = __osDisableInt();
    if (features & 1)
        D_800ABBD4->control |= 8;
    if (features & 2)
        D_800ABBD4->control &= ~8;
    if (features & 4)
        D_800ABBD4->control |= 4;
    if (features & 8)
        D_800ABBD4->control &= ~4;
    if (features & 0x10)
        D_800ABBD4->control |= 0x10;
    if (features & 0x20)
        D_800ABBD4->control &= ~0x10;
    if (features & 0x40)
        D_800ABBD4->control = (D_800ABBD4->control | 0x10000) & ~0x300;
    if (features & 0x80) {
        D_800ABBD4->control &= ~0x10000;
        D_800ABBD4->control |= D_800ABBD4->mode->control & 0x300;
    }
    D_800ABBD4->state |= 8;
    __osRestoreInt(mask);
}
