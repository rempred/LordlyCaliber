extern void __osSiGetAccess(void);
extern void __osSiRelAccess(void);
extern int __osSiRawStartDma(int direction, void *buffer);
extern int osRecvMesg(void *queue, void **message, int flags);
extern void __osPackRequestData(unsigned char command);
extern void __osContGetInitData(unsigned char *bits, void *status);
extern unsigned char __osContLastCmd;
extern unsigned char __osContPifRam[64];

int func_000205f0(void *queue)
{
    int result;
    __osSiGetAccess();
    if (__osContLastCmd != 0) {
        __osPackRequestData(0);
        __osSiRawStartDma(1, __osContPifRam);
        osRecvMesg(queue, 0, 1);
    }
    result = __osSiRawStartDma(0, __osContPifRam);
    __osContLastCmd = 0;
    __osSiRelAccess();
    return result;
}

void func_00020670(void *status)
{
    unsigned char bits;
    __osContGetInitData(&bits, status);
}
