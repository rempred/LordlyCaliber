typedef struct ViFieldView {
    unsigned int origin;
    unsigned int yScale;
    unsigned int vStart;
    unsigned int vBurst;
    unsigned int vInterrupt;
} ViFieldView;
typedef struct ViModeView {
    unsigned int type;
    unsigned int control;
    unsigned int width;
    unsigned int burst;
    unsigned int vSync;
    unsigned int hSync;
    unsigned int leap;
    unsigned int hStart;
    unsigned int xScale;
    unsigned int vCurrent;
    ViFieldView field[2];
} ViModeView;
typedef struct ViContextView {
    unsigned short state;
    unsigned short retraceCount;
    void *framebuffer;
    ViModeView *mode;
    unsigned int control;
    void *queue;
    void *message;
    unsigned char field_18[8];
    unsigned int xScale;
    float yScaleFactor;
    unsigned short field_28;
    unsigned short field_2A;
    unsigned int yScale;
} ViContextView;
extern ViContextView *D_800ABBD0, *D_800ABBD4;
extern unsigned int __additional_scanline;
extern const double D_800AEA10;
extern unsigned int osVirtualToPhysical(void *address);

void os_vi_swap_context(void)
{
    ViContextView *context = D_800ABBD4;
    unsigned int field = *(volatile unsigned int *)0xA4400010 & 1;
    ViModeView *mode = context->mode;
    unsigned int origin = osVirtualToPhysical(context->framebuffer) + mode->field[field].origin;
    unsigned int hStart, vStart;
    ViContextView *old;
    if (context->state & 2)
        context->xScale |= mode->xScale & ~0xFFF;
    else
        context->xScale = mode->xScale;
    if (context->state & 4) {
        unsigned int scale = mode->field[field].yScale & 0xFFF;
        double converted = (int)scale;
        /* Preserve the original retained unsigned-conversion constant. */
        if ((int)scale < 0)
            converted += D_800AEA10;
        context->yScale = context->yScaleFactor * (float)converted;
        context->yScale |= mode->field[field].yScale & ~0xFFF;
    } else {
        context->yScale = mode->field[field].yScale;
    }
    hStart = mode->hStart;
    vStart = mode->field[field].vStart - (__additional_scanline << 16) + __additional_scanline;
    if (context->state & 0x20)
        hStart = 0;
    if (context->state & 0x40) {
        context->yScale = 0;
        origin = osVirtualToPhysical(context->framebuffer);
    }
    if (context->state & 0x80) {
        context->yScale = (context->field_28 << 16) & 0x03FF0000;
        origin = osVirtualToPhysical(context->framebuffer);
    }
    *(volatile unsigned int *)0xA4400004 = origin;
    *(volatile unsigned int *)0xA4400008 = mode->width;
    *(volatile unsigned int *)0xA4400014 = mode->burst;
    *(volatile unsigned int *)0xA4400018 = mode->vSync;
    *(volatile unsigned int *)0xA440001C = mode->hSync;
    *(volatile unsigned int *)0xA4400020 = mode->leap;
    *(volatile unsigned int *)0xA4400024 = hStart;
    *(volatile unsigned int *)0xA4400028 = vStart;
    *(volatile unsigned int *)0xA440002C = mode->field[field].vBurst;
    *(volatile unsigned int *)0xA440000C = mode->field[field].vInterrupt;
    *(volatile unsigned int *)0xA4400030 = context->xScale;
    *(volatile unsigned int *)0xA4400034 = context->yScale;
    *(volatile unsigned int *)0xA4400000 = context->control;
    old = D_800ABBD0;
    D_800ABBD0 = context;
    D_800ABBD4 = old;
    *old = *context;
}
