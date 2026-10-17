typedef struct { unsigned int words[15]; unsigned int status; } PifRam;
typedef struct {
    unsigned char dummy, txBytes, rxBytes, command;
    unsigned char typeLow, typeHigh, status, terminal;
} QueryPacket;
typedef struct {
    unsigned short type;
    unsigned char status, error;
} ControllerStatus;
extern PifRam __osPfsPifRam;
extern unsigned char __osContLastCmd, __osMaxControllers;
extern void __osSiGetAccess(void), __osSiRelAccess(void);
extern int __osSiRawStartDma(int, void *);
extern int osRecvMesg(void *, void **, int);
void func_0002b90c(unsigned char);
void func_0002b9b0(unsigned char *, ControllerStatus *);

int func_0002b790(void *queue, unsigned char *pattern)
{
    ControllerStatus status[4];
    void *message;
    unsigned char bits;
    unsigned char resultBits = 0;
    int retry = 3;
    int result;
    int i;
    __osSiGetAccess();
    do {
        func_0002b90c(0);
        __osSiRawStartDma(1, &__osPfsPifRam);
        osRecvMesg(queue, &message, 1);
        result = __osSiRawStartDma(0, &__osPfsPifRam);
        osRecvMesg(queue, &message, 1);
        func_0002b9b0(&bits, status);
        for (i = 0; i < __osMaxControllers; i++) {
            if (!(status[i].status & 4)) {
                retry--;
                break;
            }
        }
        if (i == __osMaxControllers)
            retry = 0;
    } while (retry > 0);
    for (i = 0; i < __osMaxControllers; i++) {
        if (status[i].error == 0 && (status[i].status & 1))
            resultBits |= 1 << i;
    }
    __osSiRelAccess();
    *pattern = resultBits;
    return result;
}

void func_0002b90c(unsigned char command)
{
    unsigned char *cursor = (unsigned char *)&__osPfsPifRam;
    QueryPacket packet;
    int channel;
    __osContLastCmd = command;
    __osPfsPifRam.status = 1;
    packet.dummy = 0xFF;
    packet.txBytes = 1;
    packet.rxBytes = 3;
    packet.command = command;
    packet.typeLow = 0xFF;
    packet.typeHigh = 0xFF;
    packet.status = 0xFF;
    packet.terminal = 0xFF;
    for (channel = 0; channel < __osMaxControllers; channel++) {
        *(QueryPacket *)cursor = packet;
        cursor += sizeof(QueryPacket);
    }
    *cursor = 0xFE;
}

void func_0002b9b0(unsigned char *bits, ControllerStatus *status)
{
    unsigned char *cursor;
    QueryPacket packet;
    unsigned char pattern = 0;
    int channel;
    cursor = (unsigned char *)&__osPfsPifRam;
    for (channel = 0; channel < __osMaxControllers;
         channel++, cursor += sizeof(QueryPacket), status++) {
        packet = *(QueryPacket *)cursor;
        status->error = (packet.rxBytes & 0xC0) >> 4;
        if (status->error == 0) {
            status->type = packet.typeLow | (packet.typeHigh << 8);
            status->status = packet.status;
            pattern |= 1 << channel;
        }
    }
    *bits = pattern;
}

