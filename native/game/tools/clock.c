/* Day/night pacing for func_0004271c. The export reserves its last four bytes
 * for a format tag, a subsecond accumulator and the night/day lengths. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef struct { u16 hours; u8 minutes, seconds; } Time;
typedef struct { u16 year; u8 month, day; } Date;
typedef struct {
    Time time;
    Date date;
    u8 speed, tick;
} Clock;
extern Clock clock_state;
typedef struct { u8 phase, length[2]; } Pacing;
extern Pacing clock_pacing;
extern u8 clock_second(Time *time, u8 mode);
extern int clock_next_date(Date *date);

void clock_step(void) {
    unsigned int credit, length;
    if (++clock_state.tick < clock_state.speed)
        return;
    clock_state.tick = 0;
    /* Legacy exported games could save 60..63 in the six-bit seconds field. */
    clock_state.time.seconds = clock_state.time.seconds < 60 ? clock_state.time.seconds : 0;
    credit = clock_pacing.phase + 60;
    length = clock_pacing.length[(unsigned int)(clock_state.time.hours - 6) < 12];
    while (credit >= length) {
        credit -= length;
        if (clock_second(&clock_state.time, 1))
            clock_next_date(&clock_state.date);
    }
    clock_pacing.phase = credit;
}
