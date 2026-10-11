typedef signed char s8;
typedef signed int s32;
typedef unsigned int u32;

extern u32 func_801D96A0(u32);

u32 func_0021CA18(s32 target)
{
    volatile u32 stack_gap[2];
    u32 cursor;
    u32 previous;
    u32 result;

    previous = -1;
    cursor = 0;
    if (*(u32 *)(*(s8 **)0x801CE8C0 + 0x814) != 0) {
loop:
        result = previous;
        if (cursor != target) {
            previous = cursor;
            cursor = func_801D96A0(cursor);
            if (cursor >= *(u32 *)(*(s8 **)0x801CE8C0 + 0x814)) {
                goto done;
            }
            goto loop;
        }
    } else {
done:
        result = previous;
    }
    return result;
}
