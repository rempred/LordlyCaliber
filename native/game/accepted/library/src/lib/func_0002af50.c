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
    unsigned char remaining[32];
} ViContextView;
extern ViContextView D_800ABB70[2];
extern ViContextView *D_800ABBD0, *D_800ABBD4;
extern unsigned int osTvType;
extern ViModeView osViModePalLan1, osViModeMpalLan1, osViModeNtscLan1;
extern void bzero(void *memory, int bytes);
extern void __osViSwapContext(void);

void func_0002af50(void)
{
    bzero(D_800ABB70, sizeof(D_800ABB70));
    D_800ABBD0 = &D_800ABB70[0];
    D_800ABBD4 = &D_800ABB70[1];
    D_800ABB70[0].retraceCount = D_800ABB70[1].retraceCount = 1;
    D_800ABB70[0].framebuffer = D_800ABB70[1].framebuffer = (void *)0x80000000;
    if (osTvType == 0)
        D_800ABB70[1].mode = &osViModePalLan1;
    else if (osTvType == 2)
        D_800ABB70[1].mode = &osViModeMpalLan1;
    else
        D_800ABB70[1].mode = &osViModeNtscLan1;
    D_800ABBD4->state = 0x20;
    D_800ABBD4->control = D_800ABBD4->mode->control;
    while (*(volatile unsigned int *)0xA4400010 > 10)
        ;
    *(volatile unsigned int *)0xA4400000 = 0;
    __osViSwapContext();
}
