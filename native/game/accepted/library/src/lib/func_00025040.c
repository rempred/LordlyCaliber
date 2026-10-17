/* State store and queue yield reconstructed from ROM25040..25090. */
typedef struct Thread Thread;
struct Thread {
    Thread *next;
    int priority;
    Thread **queue;
    Thread *active_next;
    unsigned short state;
};
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);
extern Thread *__osRunningThread;
extern Thread *__osRunQueue;
extern void __osEnqueueAndYield(Thread **queue);

void func_00025040(void)
{
    unsigned int status = __osDisableInt();
    __osRunningThread->state = 2;
    __osEnqueueAndYield(&__osRunQueue);
    __osRestoreInt(status);
}
