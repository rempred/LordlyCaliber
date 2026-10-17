typedef struct ViContextView {
    unsigned short state;
    unsigned short field_02;
    unsigned char field_04[0x20];
    float yScale;
} ViContextView;
extern ViContextView *D_800ABBD4;
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);

void func_00025b80(float scale)
{
    unsigned int mask = __osDisableInt();
    D_800ABBD4->yScale = scale;
    D_800ABBD4->state |= 4;
    __osRestoreInt(mask);
}
