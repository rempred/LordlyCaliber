typedef struct {
    unsigned char dummy, txBytes, rxBytes, command;
    unsigned char addressHigh, addressLow;
    unsigned char data[32];
    unsigned char crc, terminal;
} PakWritePacket;
typedef struct { unsigned int words[15]; unsigned int status; } PifRam;
extern PifRam __osPfsPifRam;
extern unsigned char __osContLastCmd;
extern int __osPfsLastChannel;
extern void __osSiGetAccess(void), __osSiRelAccess(void);
extern int __osSiRawStartDma(int direction, void *buffer);
extern int osRecvMesg(void *queue, void **message, int flags);
extern unsigned char __osContAddressCrc(unsigned short address);
extern unsigned char __osContDataCrc(unsigned char *data);
extern int __osPfsGetStatus(void *queue, int channel);
extern void bcopy(void *source, void *destination, int bytes);

int func_000281c0(void *queue, int channel, unsigned short address,
                  unsigned char *data, int force)
{
    int retry = 2;
    int error;
    unsigned char *cursor;
    unsigned char crc;
    int i;
    if (force != 1 && address < 7 && address != 0)
        return 0;
    __osSiGetAccess();
    do {
        cursor = (unsigned char *)&__osPfsPifRam;
        if (__osContLastCmd != 3 || __osPfsLastChannel != channel) {
            __osContLastCmd = 3;
            __osPfsLastChannel = channel;
            for (i = 0; i < channel; i++)
                *cursor++ = 0;
            __osPfsPifRam.status = 1;
            ((PakWritePacket *)cursor)->dummy = 0xFF;
            ((PakWritePacket *)cursor)->txBytes = 35;
            ((PakWritePacket *)cursor)->rxBytes = 1;
            ((PakWritePacket *)cursor)->command = 3;
            ((PakWritePacket *)cursor)->crc = 0xFF;
            ((PakWritePacket *)cursor)->terminal = 0xFE;
        } else {
            cursor += channel;
        }
        ((PakWritePacket *)cursor)->addressHigh = address >> 3;
        ((PakWritePacket *)cursor)->addressLow = (address << 5) | __osContAddressCrc(address);
        bcopy(data, ((PakWritePacket *)cursor)->data, 32);
        __osSiRawStartDma(1, &__osPfsPifRam);
        crc = __osContDataCrc(data);
        osRecvMesg(queue, 0, 1);
        __osSiRawStartDma(0, &__osPfsPifRam);
        osRecvMesg(queue, 0, 1);
        error = (((PakWritePacket *)cursor)->rxBytes & 0xC0) >> 4;
        if (error == 0) {
            if (crc != ((PakWritePacket *)cursor)->crc) {
                error = __osPfsGetStatus(queue, channel);
                if (error != 0)
                    break;
                error = 4;
            }
        } else {
            error = 1;
        }
    } while (error == 4 && retry-- >= 0);
    __osSiRelAccess();
    return error;
}
