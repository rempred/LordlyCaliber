typedef struct OSThread OSThread;

struct OSThread {
    OSThread *next;
};

OSThread *__osPopThread(OSThread **queue)
{
    OSThread *head = *queue;
    register OSThread *next asm("$25") = head->next;

    *queue = next;
    return head;
}
