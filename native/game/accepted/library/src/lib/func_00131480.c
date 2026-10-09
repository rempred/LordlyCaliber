typedef unsigned char u8;
typedef signed int s32;

extern u8 D_801971FD[];

/* Keep the callers' word interface; retail consumes only its low byte. */
s32 func_00131480(void *unit, s32 value_word)
{
    u8 value = (u8)value_word;
    u8 *cursor = D_801971FD + *(u8 *)((u8 *)unit + 4) * 25;
    u8 *end = cursor + 10;

    do {
        if (*cursor != value)
            ++cursor;
        else
            return 1;
    } while ((s32)cursor < (s32)end); /* Retail compares virtual addresses signed. */
    return 0;
}
