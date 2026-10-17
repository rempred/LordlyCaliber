/* Pointer-to-link traversal follows ROM25000..25034; native tail is preserved. */
typedef struct Thread Thread;
struct Thread { Thread *next; };

void func_00025000(Thread **head, Thread *thread)
{
    Thread **link = head;
    Thread *current = *link;
    while (current != 0) {
        if (current == thread) {
            *link = current->next;
            return;
        }
        link = &current->next;
        current = *link;
    }
}
