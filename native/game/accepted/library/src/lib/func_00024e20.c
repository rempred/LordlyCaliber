/* Complete dispatch and queue paths reconstructed from ROM24E20..24F40. */
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
extern void __osEnqueueThread(Thread **queue, Thread *thread);
extern Thread *__osPopThread(Thread **queue);
extern void __osEnqueueAndYield(Thread **queue);
extern void __osDispatchThread(void);

void func_00024e20(Thread *thread)
{
    unsigned int status = __osDisableInt();
    switch (thread->state) {
    case 8:
        thread->state = 2;
        __osEnqueueThread(&__osRunQueue, thread);
        break;
    case 1:
        if (thread->queue == 0 || thread->queue == &__osRunQueue) {
            thread->state = 2;
            __osEnqueueThread(&__osRunQueue, thread);
        } else {
            thread->state = 8;
            __osEnqueueThread(thread->queue, thread);
            __osEnqueueThread(&__osRunQueue, __osPopThread(thread->queue));
        }
        break;
    }
    if (__osRunningThread == 0)
        __osDispatchThread();
    else if (__osRunningThread->priority < __osRunQueue->priority) {
        __osRunningThread->state = 2;
        __osEnqueueAndYield(&__osRunQueue);
    }
    __osRestoreInt(status);
}
