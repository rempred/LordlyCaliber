/* Complete thread removal routine reconstructed from ROM2ADC0..2AEA0. */
typedef struct Thread Thread;
struct Thread {
    Thread *next;
    int priority;
    Thread **queue;
    Thread *active_next;
    unsigned short state;
    unsigned short flags;
};
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int status);
extern Thread *__osRunningThread;
extern Thread *__osActiveQueue;
extern void __osDequeueThread(Thread **queue, Thread *thread);
extern void __osDispatchThread(void);

void func_0002adc0(Thread *thread)
{
    unsigned int status = __osDisableInt();
    Thread *previous;
    if (thread == 0)
        thread = __osRunningThread;
    else if (thread->state != 1)
        __osDequeueThread(thread->queue, thread);
    if (__osActiveQueue == thread)
        __osActiveQueue = thread->active_next;
    else {
        previous = __osActiveQueue;
        while (previous->priority != -1) {
            if (previous->active_next == thread) {
                previous->active_next = thread->active_next;
                break;
            }
            previous = previous->active_next;
        }
    }
    if (thread == __osRunningThread)
        __osDispatchThread();
    __osRestoreInt(status);
}
