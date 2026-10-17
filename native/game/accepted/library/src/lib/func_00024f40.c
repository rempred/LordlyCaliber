/* Complete state switch reconstructed from ROM24F40..25000. */
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
extern void __osDequeueThread(Thread **queue, Thread *thread);
extern void __osEnqueueAndYield(Thread **queue);

void func_00024f40(Thread *thread)
{
    unsigned int status = __osDisableInt();
    unsigned short state = thread == 0 ? 4 : thread->state;
    switch (state) {
    case 4:
        __osRunningThread->state = 1;
        __osEnqueueAndYield(0);
        break;
    case 2:
    case 8:
        thread->state = 1;
        __osDequeueThread(thread->queue, thread);
        break;
    }
    __osRestoreInt(status);
}
