typedef struct SpTaskView {
    unsigned int field_00;
    unsigned int flags;
} SpTaskView;
extern unsigned int __osSpGetStatus(void);

unsigned int func_00024060(SpTaskView *task)
{
    unsigned int status = __osSpGetStatus();
    unsigned int yielded = status >> 8;
    yielded &= 1;
    if (status & 0x80) {
        task->flags |= yielded;
        task->flags &= ~2;
    }
    return yielded;
}
