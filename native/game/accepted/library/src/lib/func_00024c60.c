/* Saved-context offsets and initialization derived from ROM24C60..24D50. */
typedef struct Thread Thread;
struct Thread {
    Thread *next;
    int priority;
    Thread **queue;
    Thread *active_next;
    unsigned short state;
    unsigned short flags;
    int id;
    int fp;
    unsigned char unknown_1c[0x1C];
    long long argument;
    unsigned char unknown_40[0xB0];
    long long stack;
    long long unknown_f8;
    long long return_address;
    unsigned char unknown_108[0x10];
    unsigned int status;
    void (*entry)(void *);
    unsigned int unknown_120[2];
    unsigned int rcp_mask;
    unsigned int fp_control;
};
extern void __osCleanupThread(void);
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int status);
extern Thread *__osActiveQueue;
extern Thread *__osRunningThread;

void func_00024C60(Thread *thread, int id, void (*entry)(void *), void *argument,
                  void *stack, int priority)
{
    unsigned int status;
    thread->id = id;
    thread->next = 0;
    thread->queue = 0;
    thread->entry = entry;
    thread->argument = (long long)argument;
    thread->priority = priority;
    thread->stack = (long long)stack - 16;
    thread->return_address = (long long)__osCleanupThread;
    thread->status = 0xFF03;
    thread->rcp_mask = 0x3F;
    thread->fp_control = 0x01000800;
    thread->fp = 0;
    thread->state = 1;
    thread->flags = 0;
    status = __osDisableInt();
    thread->active_next = __osActiveQueue;
    __osActiveQueue = thread;
    __osRestoreInt(status);
}

static int func_00024D30(Thread *thread)
{
    if (thread == 0)
        thread = __osRunningThread;
    return thread->priority;
}
