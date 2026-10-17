typedef struct { unsigned int words[15]; unsigned int status; } PifRam;
typedef struct {
    unsigned char txBytes, rxBytes, command, typeLow, typeHigh, status;
} OneChannelPacket;
typedef struct {
    unsigned short type;
    unsigned char status, error;
} ControllerStatus;
extern PifRam __osPfsPifRam;
extern unsigned char __osContLastCmd, __osPfsInodeCacheBank;
extern int __osSiRawStartDma(int, void *);
extern int osRecvMesg(void *, void **, int);
void func_000273b0(int, unsigned char);
void func_00027444(int, ControllerStatus *);

int func_000272c0(void *queue, int channel)
{
    ControllerStatus status;
    void *message;
    int result;
    __osPfsInodeCacheBank = 250;
    func_000273b0(channel, 0);
    __osSiRawStartDma(1, &__osPfsPifRam);
    osRecvMesg(queue, &message, 1);
    result = __osSiRawStartDma(0, &__osPfsPifRam);
    osRecvMesg(queue, &message, 1);
    func_00027444(channel, &status);
    if ((status.status & 1) && (status.status & 2))
        return 2;
    if (status.error != 0 || !(status.status & 1))
        return 1;
    if (status.status & 4)
        return 4;
    return result;
}

void func_000273b0(int channel, unsigned char command)
{
    unsigned char *cursor;
    OneChannelPacket packet;
    int i;
    __osContLastCmd = 0xFE;
    __osPfsPifRam.status = 1;
    cursor = (unsigned char *)&__osPfsPifRam;
    packet.txBytes = 1;
    packet.rxBytes = 3;
    packet.command = command;
    packet.typeLow = 0xFF;
    packet.typeHigh = 0xFF;
    packet.status = 0xFF;
    for (i = 0; i < channel; i++)
        *cursor++ = 0;
    *(OneChannelPacket *)cursor = packet;
    cursor += sizeof(OneChannelPacket);
    *cursor = 0xFE;
}

void func_00027444(int channel, ControllerStatus *status)
{
    unsigned char *cursor = (unsigned char *)&__osPfsPifRam;
    OneChannelPacket packet;
    int i;
    for (i = 0; i < channel; i++)
        cursor++;
    packet = *(OneChannelPacket *)cursor;
    status->error = (packet.rxBytes & 0xC0) >> 4;
    if (status->error == 0) {
        status->type = packet.typeLow | (packet.typeHigh << 8);
        status->status = packet.status;
    }
}
