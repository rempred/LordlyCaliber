typedef struct SpTaskView {
    unsigned int type;
    unsigned int flags;
    void *field_08;
    unsigned int field_0C;
    void *field_10;
    unsigned int field_14;
    void *field_18;
    unsigned int field_1C;
    void *field_20;
    unsigned int field_24;
    void *outputBuffer;
    unsigned long long *outputSize;
    void *field_30;
    unsigned int field_34;
    void *field_38;
    unsigned int field_3C;
} SpTaskView;
extern SpTaskView D_800C3560;
extern void bcopy(const void *source, void *destination, int bytes);
extern unsigned int osVirtualToPhysical(void *address);
extern void osWritebackDCache(void *address, int bytes);
extern void __osSpSetStatus(unsigned int status);
extern int __osSpSetPc(unsigned int pc);
extern int __osSpRawStartDma(int direction, unsigned int spAddress, void *dramAddress, unsigned int bytes);
extern int __osSpDeviceBusy(void);

void func_00023e00(SpTaskView *task)
{
    SpTaskView *temporary = &D_800C3560;
    bcopy(task, temporary, sizeof(D_800C3560));
    if (D_800C3560.field_10)
        D_800C3560.field_10 = (void *)osVirtualToPhysical(D_800C3560.field_10);
    if (D_800C3560.field_18)
        D_800C3560.field_18 = (void *)osVirtualToPhysical(D_800C3560.field_18);
    if (D_800C3560.field_20)
        D_800C3560.field_20 = (void *)osVirtualToPhysical(D_800C3560.field_20);
    if (D_800C3560.outputBuffer)
        D_800C3560.outputBuffer = (void *)osVirtualToPhysical(D_800C3560.outputBuffer);
    if (D_800C3560.outputSize)
        D_800C3560.outputSize = (unsigned long long *)osVirtualToPhysical(D_800C3560.outputSize);
    if (D_800C3560.field_30)
        D_800C3560.field_30 = (void *)osVirtualToPhysical(D_800C3560.field_30);
    if (D_800C3560.field_38)
        D_800C3560.field_38 = (void *)osVirtualToPhysical(D_800C3560.field_38);
    if (D_800C3560.flags & 1) {
        D_800C3560.field_18 = D_800C3560.field_38;
        D_800C3560.field_1C = D_800C3560.field_3C;
        task->flags &= ~1;
        if (D_800C3560.flags & 4)
            D_800C3560.field_10 = *(void **)((unsigned int)((char *)task->field_38 + 0xBFC) | 0xA0000000);
    }
    osWritebackDCache(temporary, sizeof(D_800C3560));
    __osSpSetStatus(0x2B00);
    while (__osSpSetPc(0x04001000) == -1)
        ;
    while (__osSpRawStartDma(1, 0x04000FC0, temporary, sizeof(D_800C3560)) == -1)
        ;
    while (__osSpDeviceBusy())
        ;
    while (__osSpRawStartDma(1, 0x04001000, temporary->field_08, temporary->field_0C) == -1)
        ;
}

void func_0002400c(SpTaskView *task)
{
    while (__osSpDeviceBusy())
        ;
    __osSpSetStatus(0x125);
}
