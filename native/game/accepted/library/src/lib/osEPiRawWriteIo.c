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
} PiHandleView;
extern PiHandleView *__osCurrentHandle[];

int osEPiRawWriteIo(PiHandleView *handle, unsigned int address, unsigned int value)
{
    PiHandleView *current;
    unsigned int domain;
    while (*(volatile unsigned int *)0xA4600010 & 3)
        ;
    domain = handle->domain;
    if (__osCurrentHandle[domain]->type != handle->type) {
        current = __osCurrentHandle[domain];
        if (domain == 0) {
            if (current->latency != handle->latency)
                *(volatile unsigned int *)0xA4600014 = handle->latency;
            if (current->pageSize != handle->pageSize)
                *(volatile unsigned int *)0xA460001C = handle->pageSize;
            if (current->releaseDuration != handle->releaseDuration)
                *(volatile unsigned int *)0xA4600020 = handle->releaseDuration;
            if (current->pulse != handle->pulse)
                *(volatile unsigned int *)0xA4600018 = handle->pulse;
        } else {
            if (current->latency != handle->latency)
                *(volatile unsigned int *)0xA4600024 = handle->latency;
            if (current->pageSize != handle->pageSize)
                *(volatile unsigned int *)0xA460002C = handle->pageSize;
            if (current->releaseDuration != handle->releaseDuration)
                *(volatile unsigned int *)0xA4600030 = handle->releaseDuration;
            if (current->pulse != handle->pulse)
                *(volatile unsigned int *)0xA4600028 = handle->pulse;
        }
        current->type = handle->type;
        current->latency = handle->latency;
        current->pageSize = handle->pageSize;
        current->releaseDuration = handle->releaseDuration;
        current->pulse = handle->pulse;
    }
    *(volatile unsigned int *)(handle->baseAddress | address | 0xA0000000) = value;
    return 0;
}
