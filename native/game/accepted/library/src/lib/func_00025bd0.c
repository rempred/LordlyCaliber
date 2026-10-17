typedef struct ViContextView {
    unsigned short state;
    unsigned short field_02;
    void *framebuffer;
} ViContextView;
extern ViContextView *D_800ABBD4;
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);

void func_00025bd0(void *framebuffer)
{
    unsigned int mask = __osDisableInt();
    D_800ABBD4->framebuffer = framebuffer;
    D_800ABBD4->state |= 0x10;
    __osRestoreInt(mask);
}
