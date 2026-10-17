typedef struct ViContextView {
    unsigned short state;
} ViContextView;
extern ViContextView *D_800ABBD4;
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);

void func_00025f30(unsigned char black)
{
    unsigned int mask = __osDisableInt();
    if (black)
        D_800ABBD4->state |= 0x20;
    else
        D_800ABBD4->state &= ~0x20;
    __osRestoreInt(mask);
}
