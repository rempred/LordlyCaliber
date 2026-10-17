/* Complete timer producer reconstructed from ROM251E0..254E0.
 * The list pointer stays in the original data owner at RAM800AA7B0;
 * this producer introduces no second copy of its initialized storage.
 */
typedef unsigned long long TimerTicks;
typedef struct Timer Timer;
struct Timer {
    Timer *next;
    Timer *prev;
    TimerTicks interval;
    TimerTicks value;
    void *queue;
    void *message;
};
extern Timer *__osTimerList;
extern TimerTicks __osCurrentTime;
extern unsigned int __osBaseCounter;
extern unsigned int __osViIntrCount;
extern unsigned int __osTimerCounter;
extern unsigned int osGetCount(void);
extern void __osSetCompare(unsigned int count);
extern int osSendMesg(void *queue, void *message, int flags);
extern unsigned int __osDisableInt(void);
extern void __osRestoreInt(unsigned int mask);
void func_00025370(TimerTicks value);
TimerTicks func_000253d0(Timer *timer);

void func_000251e0(void)
{
    __osCurrentTime = 0;
    __osBaseCounter = 0;
    __osViIntrCount = 0;
    __osTimerList->interval = __osTimerList->value = 0;
    __osTimerList->next = __osTimerList->prev = __osTimerList;
    __osTimerList->queue = 0;
    __osTimerList->message = 0;
}

void func_00025234(void)
{
    Timer *timer;
    unsigned int count;
    TimerTicks elapsed;
    if (__osTimerList->next == __osTimerList)
        return;
    for (;;) {
        timer = __osTimerList->next;
        if (timer == __osTimerList) {
            __osSetCompare(0);
            __osTimerCounter = 0;
            break;
        }
        count = osGetCount();
        elapsed = count - __osTimerCounter;
        __osTimerCounter = count;
        if (elapsed < timer->value) {
            timer->value -= elapsed;
            func_00025370(timer->value);
            break;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        timer->next = 0;
        timer->prev = 0;
        if (timer->queue != 0)
            osSendMesg(timer->queue, timer->message, 0);
        if (timer->interval != 0) {
            timer->value = timer->interval;
            func_000253d0(timer);
        }
    }
}

void func_00025370(TimerTicks value)
{
    unsigned int status = __osDisableInt();
    unsigned int count = osGetCount();
    TimerTicks deadline;
    /* Keep the named 64-bit accumulator through the addition. Narrowing
     * earlier changes the pinned compiler's frame and argument registers. */
    deadline = count;
    __osTimerCounter = count;
    deadline += value;
    __osSetCompare(deadline);
    __osRestoreInt(status);
}

TimerTicks func_000253d0(Timer *timer)
{
    unsigned int status = __osDisableInt();
    Timer *position = __osTimerList->next;
    TimerTicks remaining = timer->value;
    while (position != __osTimerList && position->value < remaining) {
        remaining -= position->value;
        position = position->next;
    }
    timer->value = remaining;
    if (position != __osTimerList)
        position->value -= remaining;
    timer->next = position;
    timer->prev = position->prev;
    position->prev->next = timer;
    position->prev = timer;
    __osRestoreInt(status);
    return remaining;
}
