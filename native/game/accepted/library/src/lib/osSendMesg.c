/* Independently reconstructed from ROM 23C10..23D40 and its callees. */
typedef struct MessageThread MessageThread;
struct MessageThread {
    MessageThread *next;
    int unknown_04[3];
    unsigned short state_10;
};
typedef struct MessageQueue {
    MessageThread *receive_waiters;
    MessageThread *send_waiters;
    int valid_count;
    int first;
    int capacity;
    void **messages;
} MessageQueue;

extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);
extern MessageThread *__osRunningThread;
extern void __osEnqueueAndYield(MessageThread **queue);
extern MessageThread *__osPopThread(MessageThread **queue);
extern void osStartThread(MessageThread *thread);

int osSendMesg(MessageQueue *queue, void *message, int flags)
{
    unsigned int mask = __osDisableInt();
    int index;

    while (queue->valid_count >= queue->capacity) {
        if (flags != 1) {
            __osRestoreInt(mask);
            return -1;
        }
        __osRunningThread->state_10 = 8;
        __osEnqueueAndYield(&queue->send_waiters);
    }
    index = (queue->first + queue->valid_count) % queue->capacity;
    queue->messages[index] = message;
    queue->valid_count++;
    if (queue->receive_waiters->next != 0) {
        osStartThread(__osPopThread(&queue->receive_waiters));
    }
    __osRestoreInt(mask);
    return 0;
}
