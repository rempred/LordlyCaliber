typedef struct {
    unsigned char dummy, txBytes, rxBytes, command;
    unsigned short buttons;
    signed char stickX, stickY;
} ReadPacket;
typedef struct {
    unsigned short buttons;
    signed char stickX, stickY;
    unsigned char error;
} PadData;
typedef struct { unsigned int words[15]; unsigned int status; } PifRam;

extern PifRam __osContPifRam;
extern unsigned char __osContLastCmd, __osMaxControllers;
extern void __osSiGetAccess(void), __osSiRelAccess(void);
extern int __osSiRawStartDma(int direction, void *buffer);
extern int osRecvMesg(void *queue, void **message, int flags);
void func_000207b4(void);

int func_00020690(void *queue)
{
    int result;
    __osSiGetAccess();
    if (__osContLastCmd != 1) {
        func_000207b4();
        __osSiRawStartDma(1, &__osContPifRam);
        osRecvMesg(queue, 0, 1);
    }
    result = __osSiRawStartDma(0, &__osContPifRam);
    __osContLastCmd = 1;
    __osSiRelAccess();
    return result;
}

void func_00020718(PadData *pad)
{
    unsigned char *cursor = (unsigned char *)&__osContPifRam;
    ReadPacket packet;
    int channel;
    for (channel = 0; channel < __osMaxControllers; channel++, cursor += sizeof(ReadPacket), pad++) {
        packet = *(ReadPacket *)cursor;
        pad->error = (packet.rxBytes & 0xC0) >> 4;
        if (pad->error == 0) {
            pad->buttons = packet.buttons;
            pad->stickX = packet.stickX;
            pad->stickY = packet.stickY;
        }
    }
}

void func_000207b4(void)
{
    unsigned char *cursor = (unsigned char *)&__osContPifRam;
    ReadPacket packet;
    int channel;
    for (channel = 0; channel < 15; channel++)
        __osContPifRam.words[channel] = 0;
    __osContPifRam.status = 1;
    packet.dummy = 0xFF;
    packet.txBytes = 1;
    packet.rxBytes = 4;
    packet.command = 1;
    packet.buttons = 0xFFFF;
    packet.stickX = -1;
    packet.stickY = -1;
    for (channel = 0; channel < __osMaxControllers; channel++) {
        *(ReadPacket *)cursor = packet;
        cursor += sizeof(ReadPacket);
    }
    *cursor = 0xFE;
}
