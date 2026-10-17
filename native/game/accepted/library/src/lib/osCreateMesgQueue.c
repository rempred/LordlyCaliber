/* Layout and stores independently derived from the Rev 0 queue routines. */
typedef struct MessageThread MessageThread;
typedef struct MessageQueue {
    MessageThread *receive_waiters;
    MessageThread *send_waiters;
    int valid_count;
    int first;
    int capacity;
    void **messages;
} MessageQueue;

extern MessageThread g_os_thread_queue_sentinel;

void func_00023970(MessageQueue *queue, void **messages, int capacity)
{
    queue->receive_waiters = &g_os_thread_queue_sentinel;
    queue->send_waiters = &g_os_thread_queue_sentinel;
    queue->valid_count = 0;
    queue->first = 0;
    queue->capacity = capacity;
    queue->messages = messages;
}

