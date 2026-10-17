typedef struct {
    unsigned char dummy, txBytes, rxBytes, command;
    unsigned char addressHigh, addressLow;
    unsigned char data[32];
    unsigned char crc, terminal;
} PakReadPacket;
typedef struct { unsigned int words[15]; unsigned int status; } PifRam;
extern PifRam __osPfsPifRam;
extern unsigned char __osContLastCmd;
extern int __osPfsLastChannel;
extern void __osSiGetAccess(void), __osSiRelAccess(void);
extern int __osSiRawStartDma(int, void *);
extern int osRecvMesg(void *, void **, int);
extern unsigned char __osContAddressCrc(unsigned short);
extern unsigned char __osContDataCrc(unsigned char *);
extern int __osPfsGetStatus(void *, int);
extern void bcopy(void *, void *, int);

int func_00027fd0(void *queue, int channel, unsigned short address,
                  unsigned char *data)
{
    int retry = 2;
    int error;
    unsigned char *cursor;
    int i;
    __osSiGetAccess();
    do {
        cursor = (unsigned char *)&__osPfsPifRam;
        if (__osContLastCmd != 2 || __osPfsLastChannel != channel) {
            __osContLastCmd = 2;
            __osPfsLastChannel = channel;
            for (i = 0; i < channel; i++)
                *cursor++ = 0;
            __osPfsPifRam.status = 1;
            ((PakReadPacket *)cursor)->dummy = 0xFF;
            ((PakReadPacket *)cursor)->txBytes = 3;
            ((PakReadPacket *)cursor)->rxBytes = 33;
            ((PakReadPacket *)cursor)->command = 2;
            ((PakReadPacket *)cursor)->crc = 0xFF;
            ((PakReadPacket *)cursor)->terminal = 0xFE;
        } else {
            cursor += channel;
        }
        ((PakReadPacket *)cursor)->addressHigh = address >> 3;
        ((PakReadPacket *)cursor)->addressLow = (address << 5) | __osContAddressCrc(address);
        __osSiRawStartDma(1, &__osPfsPifRam);
        osRecvMesg(queue, 0, 1);
        __osSiRawStartDma(0, &__osPfsPifRam);
        osRecvMesg(queue, 0, 1);
        error = (((PakReadPacket *)cursor)->rxBytes & 0xC0) >> 4;
        if (error == 0) {
            if (__osContDataCrc(((PakReadPacket *)cursor)->data) != ((PakReadPacket *)cursor)->crc) {
                error = __osPfsGetStatus(queue, channel);
                if (error != 0)
                    break;
                error = 4;
            } else {
                bcopy(((PakReadPacket *)cursor)->data, data, 32);
            }
        } else {
            error = 1;
        }
    } while (error == 4 && retry-- >= 0);
    __osSiRelAccess();
    return error;
}
