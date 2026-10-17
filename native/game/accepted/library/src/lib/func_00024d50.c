/* Queue and state transitions reconstructed from ROM24D50..24E20. */
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
extern void __osRestoreInt(unsigned int mask);
extern Thread *__osRunningThread;
extern Thread *__osRunQueue;
extern void __osDequeueThread(Thread **queue, Thread *thread);
extern void __osEnqueueThread(Thread **queue, Thread *thread);
extern void __osEnqueueAndYield(Thread **queue);

void func_00024d50(Thread *thread, int priority)
{
    unsigned int status = __osDisableInt();
    if (thread == 0)
        thread = __osRunningThread;
    if (thread->priority != priority) {
        thread->priority = priority;
        if (thread != __osRunningThread && thread->state != 1) {
            __osDequeueThread(thread->queue, thread);
            __osEnqueueThread(thread->queue, thread);
        }
        if (__osRunningThread->priority < __osRunQueue->priority) {
            __osRunningThread->state = 2;
            __osEnqueueAndYield(&__osRunQueue);
        }
    }
    __osRestoreInt(status);
}
