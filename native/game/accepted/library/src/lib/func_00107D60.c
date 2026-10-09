typedef signed int s32;
typedef unsigned int u32;
extern char D_801EE0A0[];
extern void func_00023940(const char *format, ...);
extern void *resource_alloc(u32 bytes);

s32 *func_00107D60(s32 first, s32 last, s32 *predecessors)
{
    s32 count;
    s32 current;
    s32 *list;

    if (first == last) {
        count = 1;
    } else {
        current = last;
        count = 2;
        for (;;) {
            current = predecessors[current];
            if (current >= 0x1000) {
                func_00023940(D_801EE0A0, current);
                /* The original diagnostic return enters a permanent self-loop. */
                for (;;) {}
            }
            if (current == first)
                break;
            ++count;
        }
    }

    list = resource_alloc((count + 1) * 4);
    list[count] = -1;
    --count;
    current = last;
    while (count >= 0) {
        list[count] = current;
        current = predecessors[current];
        --count;
    }
    return list;
}
