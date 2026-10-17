/* Event table stores and pre-NMI registration follow ROM 0x23D40. */
typedef struct EventState {
    void *queue;
    void *message;
} EventState;

extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int status);
extern int osSendMesg(void *queue, void *message, int flags);
extern EventState __osEventStateTab[];
extern int __osShutdown;
extern int D_800AA760;

void func_00023d40(unsigned int event, void *queue, void *message)
{
    unsigned int status = __osDisableInt();
    EventState *state = &__osEventStateTab[event];

    state->queue = queue;
    state->message = message;
    if (event == 14) {
        if (__osShutdown && !D_800AA760)
            osSendMesg(queue, message, 0);
        D_800AA760 = 1;
    }
    __osRestoreInt(status);
}
