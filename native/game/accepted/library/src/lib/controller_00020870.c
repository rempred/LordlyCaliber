typedef unsigned long long Ticks;
typedef struct Timer Timer;
struct Timer {
    Timer *next, *prev;
    Ticks interval, value;
    void *queue, *message;
};
typedef struct {
    void *receiveWaiters, *sendWaiters;
    int validCount, first, capacity;
    void **messages;
} MessageQueue;
typedef struct {
    unsigned char dummy, txBytes, rxBytes, command;
    unsigned char typeLow, typeHigh, status, terminal;
} QueryPacket;
typedef struct {
    unsigned short type;
    unsigned char status, error;
} ControllerStatus;
typedef struct { unsigned int words[15]; unsigned int status; } PifRam;
extern PifRam __osContPifRam;
extern int __osContinitialized;
extern unsigned char __osContLastCmd, __osMaxControllers;
extern MessageQueue __osEepromTimerQ;
extern void *__osEepromTimerMsg;
extern Ticks osGetTime(void);
extern void osCreateMesgQueue(MessageQueue *, void **, int);
extern int osSetTimer(Timer *, Ticks, Ticks, MessageQueue *, void *);
extern int osRecvMesg(MessageQueue *, void **, int);
extern int __osSiRawStartDma(int, void *);
extern void __osSiCreateAccessQueue(void);
void func_00020a0c(unsigned char *, ControllerStatus *);
void func_00020ac0(unsigned char);

int func_00020870(MessageQueue *queue, unsigned char *bits, ControllerStatus *status)
{
    Ticks now;
    Timer timer;
    MessageQueue timerQueue;
    void *message;
    int result;
    if (__osContinitialized)
        return 0;
    __osContinitialized = 1;
    now = osGetTime();
    if (now < 23437500ULL) {
        osCreateMesgQueue(&timerQueue, &message, 1);
        osSetTimer(&timer, 23437500ULL - now, 0, &timerQueue, &message);
        osRecvMesg(&timerQueue, &message, 1);
    }
    __osMaxControllers = 4;
    func_00020ac0(0);
    __osSiRawStartDma(1, &__osContPifRam);
    osRecvMesg(queue, &message, 1);
    result = __osSiRawStartDma(0, &__osContPifRam);
    osRecvMesg(queue, &message, 1);
    func_00020a0c(bits, status);
    __osContLastCmd = 0;
    __osSiCreateAccessQueue();
    osCreateMesgQueue(&__osEepromTimerQ, &__osEepromTimerMsg, 1);
    return result;
}

void func_00020a0c(unsigned char *bits, ControllerStatus *status)
{
    unsigned char *cursor;
    QueryPacket packet;
    unsigned char pattern = 0;
    int channel;
    cursor = (unsigned char *)&__osContPifRam;
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

void func_00020ac0(unsigned char command)
{
    unsigned char *cursor;
    QueryPacket packet;
    int channel;
    for (channel = 0; channel < 15; channel++)
        __osContPifRam.words[channel] = 0;
    __osContPifRam.status = 1;
    cursor = (unsigned char *)&__osContPifRam;
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
