typedef struct PiHandleView {
    struct PiHandleView *next;
    unsigned char type;
    unsigned char latency;
    unsigned char pageSize;
    unsigned char releaseDuration;
    unsigned char pulse;
    unsigned char domain;
    unsigned short field_0A;
    unsigned int baseAddress;
    unsigned int speed;
    unsigned char transfer[0x60];
} PiHandleView;
extern PiHandleView __CartRomHandle;
extern PiHandleView *__osPiTable;
extern int D_800AA430;
extern void __osPiGetAccess(void);
extern void __osPiRelAccess(void);
extern void bzero(void *memory, int bytes);
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);

PiHandleView *func_0001c130(void)
{
    unsigned int latency, pageSize, releaseDuration, pulse, config, mask;
    __osPiGetAccess();
    if (!D_800AA430) {
        __osPiRelAccess();
        return &__CartRomHandle;
    }
    /* The early return keeps initialization separate from handle reuse. */
    D_800AA430 = 0;
    __CartRomHandle.type = 0;
    __CartRomHandle.baseAddress = 0xB0000000;
    __CartRomHandle.domain = 0;
    __CartRomHandle.speed = 0;
    bzero(&__CartRomHandle.transfer, 0x60);
    while (*(volatile unsigned int *)0xA4600010 & 3)
        ;
    latency = *(volatile unsigned int *)0xA4600014;
    pageSize = *(volatile unsigned int *)0xA460001C;
    releaseDuration = *(volatile unsigned int *)0xA4600020;
    pulse = *(volatile unsigned int *)0xA4600018;
    *(volatile unsigned int *)0xA4600014 = 0xFF;
    *(volatile unsigned int *)0xA460001C = 0;
    *(volatile unsigned int *)0xA4600020 = 3;
    *(volatile unsigned int *)0xA4600018 = 0xFF;
    config = *(volatile unsigned int *)(__CartRomHandle.baseAddress | 0xA0000000);
    *(volatile unsigned int *)0xA4600014 = latency;
    *(volatile unsigned int *)0xA460001C = pageSize;
    *(volatile unsigned int *)0xA4600020 = releaseDuration;
    *(volatile unsigned int *)0xA4600018 = pulse;
    __CartRomHandle.latency = config & 0xFF;
    __CartRomHandle.pageSize = (config >> 16) & 0xF;
    __CartRomHandle.releaseDuration = (config >> 20) & 0xF;
    __CartRomHandle.pulse = (config >> 8) & 0xFF;
    mask = __osDisableInt();
    __CartRomHandle.next = __osPiTable;
    __osPiTable = &__CartRomHandle;
    __osRestoreInt(mask);
    __osPiRelAccess();
    return &__CartRomHandle;
}
