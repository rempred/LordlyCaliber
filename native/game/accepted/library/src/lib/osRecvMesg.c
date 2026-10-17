/* Independently reconstructed from ROM 23AE0..23C10 and its callees. */
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

int osRecvMesg(MessageQueue *queue, void **message, int flags)
{
    unsigned int mask = __osDisableInt();

    while (queue->valid_count == 0) {
        if (flags == 0) {
            __osRestoreInt(mask);
            return -1;
        }
        __osRunningThread->state_10 = 8;
        __osEnqueueAndYield(&queue->receive_waiters);
    }
    if (message != 0) {
        *message = queue->messages[queue->first];
    }
    queue->first = (queue->first + 1) % queue->capacity;
    queue->valid_count--;
    if (queue->send_waiters->next != 0) {
        osStartThread(__osPopThread(&queue->send_waiters));
    }
    __osRestoreInt(mask);
    return 0;
}
