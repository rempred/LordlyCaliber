/* Timer fields and list insertion reconstructed from ROM25140..251E0. */
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
extern TimerTicks __osInsertTimer(Timer *timer);
extern void __osSetTimerIntr(TimerTicks value);

int func_00025140(Timer *timer, TimerTicks countdown, TimerTicks interval,
                  void *queue, void *message)
{
    TimerTicks next;
    timer->next = 0;
    timer->prev = 0;
    timer->value = countdown;
    timer->interval = interval;
    if (countdown == 0)
        timer->value = interval;
    timer->queue = queue;
    timer->message = message;
    next = __osInsertTimer(timer);
    if (__osTimerList->next == timer)
        __osSetTimerIntr(next);
    return 0;
}
