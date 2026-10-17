typedef struct MesgQueue MesgQueue;
typedef struct PiBufferView {
    unsigned int field_00;
    unsigned int field_04;
    unsigned int field_08;
    unsigned int field_0C;
    unsigned int remaining[5];
} PiBufferView;
typedef struct PiTransferView {
    unsigned int type;
    unsigned short field_04;
    unsigned short bufferIndex;
    int field_08;
    unsigned int field_0C;
    unsigned int field_10;
    unsigned int field_14;
    PiBufferView buffer[2];
} PiTransferView;
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
    PiTransferView transfer;
} PiHandleView;
typedef struct DmaMessageView {
    unsigned short type;
    unsigned char priority;
    unsigned char status;
    MesgQueue *returnQueue;
    void *dramAddress;
    unsigned int deviceAddress;
    unsigned int bytes;
    PiHandleView *handle;
} DmaMessageView;
typedef struct DevMgrView {
    int active;
    void *thread;
    MesgQueue *commandQueue;
    MesgQueue *eventQueue;
    MesgQueue *accessQueue;
    int (*dma)(int, unsigned int, void *, unsigned int);
    int (*epiDma)(PiHandleView *, int, unsigned int, void *, unsigned int);
} DevMgrView;
extern int osRecvMesg(MesgQueue *, void **, int);
extern int osSendMesg(MesgQueue *, void *, int);
extern void __osResetGlobalIntMask(unsigned int);
extern void __osSetGlobalIntMask(unsigned int);
extern int __osEPiRawWriteIo(PiHandleView *, unsigned int, unsigned int);
extern int __osEPiRawReadIo(PiHandleView *, unsigned int, unsigned int *);
extern void osYieldThread(void);

void func_0001c2b0(void *argument)
{
    DevMgrView *manager = argument;
    DmaMessageView *message = 0;
    void *accessMessage;
    void *eventMessage;
    unsigned int data;
    PiHandleView *handle;
    PiBufferView *buffer;
    PiTransferView *transfer;
    unsigned int twoPhase;
    int status;
    for (;;) {
        osRecvMesg(manager->commandQueue, (void **)&message, 1);
        handle = message->handle;
        if (handle && handle->type == 2 && handle->transfer.type < 2) {
            handle->transfer.field_08 = -1;
            transfer = &handle->transfer;
            buffer = &transfer->buffer[transfer->bufferIndex];
            if (handle->transfer.field_04 != 3)
                buffer->field_04 -= buffer->field_0C;
            twoPhase = 0;
            if (handle->transfer.field_04 == 2)
                twoPhase = message->handle->transfer.type == 0;
            osRecvMesg(manager->accessQueue, &accessMessage, 1);
            __osResetGlobalIntMask(0x00100401);
            __osEPiRawWriteIo(message->handle, 0x05000510, handle->transfer.field_10 | 0x80000000);
            /* This is one conditional retry. A C loop hoists the PI register
             * address into a saved register and changes the native frame use. */
receive_event:
            {
                osRecvMesg(manager->eventQueue, &eventMessage, 1);
                handle = message->handle;
                transfer = &handle->transfer;
                buffer = &transfer->buffer[transfer->bufferIndex];
                if (buffer->field_00 == 0x1D) {
                    __osEPiRawWriteIo(handle, 0x05000510, handle->transfer.field_10 | 0x10000000);
                    __osEPiRawWriteIo(message->handle, 0x05000510, handle->transfer.field_10);
                    __osEPiRawReadIo(message->handle, 0x05000508, &data);
                    if (data & 0x02000000)
                        __osEPiRawWriteIo(message->handle, 0x05000510, handle->transfer.field_10 | 0x01000000);
                    buffer->field_00 = 4;
                    *(volatile unsigned int *)0xA4600010 = 2;
                    __osSetGlobalIntMask(0x00100C01);
                }
                osSendMesg(message->returnQueue, message, 0);
                if (twoPhase == 1) {
                    twoPhase = 0;
                    if (message->handle->transfer.buffer[0].field_00 == 0)
                        goto receive_event;
                }
            }
            osSendMesg(manager->accessQueue, 0, 0);
            if (message->handle->transfer.bufferIndex == 1)
                osYieldThread();
            continue;
        }
        switch (message->type) {
        case 11:
            osRecvMesg(manager->accessQueue, &accessMessage, 1);
            status = manager->dma(0, message->deviceAddress, message->dramAddress, message->bytes);
            break;
        case 12:
            osRecvMesg(manager->accessQueue, &accessMessage, 1);
            status = manager->dma(1, message->deviceAddress, message->dramAddress, message->bytes);
            break;
        case 15:
            osRecvMesg(manager->accessQueue, &accessMessage, 1);
            status = manager->epiDma(message->handle, 0, message->deviceAddress, message->dramAddress, message->bytes);
            break;
        case 16:
            osRecvMesg(manager->accessQueue, &accessMessage, 1);
            status = manager->epiDma(message->handle, 1, message->deviceAddress, message->dramAddress, message->bytes);
            break;
        case 10:
            osSendMesg(message->returnQueue, message, 0);
            /* This request does not start a DMA operation. */
        default:
            status = -1;
            break;
        }
        if (status == 0) {
            osRecvMesg(manager->eventQueue, &eventMessage, 1);
            osSendMesg(message->returnQueue, message, 0);
            osSendMesg(manager->accessQueue, 0, 0);
        }
    }
}
