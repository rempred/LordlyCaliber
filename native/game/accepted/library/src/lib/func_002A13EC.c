typedef signed short s16;

extern s16 *D_8022A97C;

int func_002A13EC(int arg0, int arg1, int arg2, int arg3)
{
    s16 *base;
    s16 *table;
    int index;
    register unsigned int next asm("$2");
    int sentinel;
    s16 current;
    int target;

    arg0--;
    base = D_8022A97C;
    table = base + base[(s16)(arg0 * 2)];
    index = 0;
    if (*table == -1) {
        goto fail;
    }

    target = arg1 * 10 + arg2;
    sentinel = -1;
loop:
    current = (s16)index;
    if (target != table[current]) {
        goto advance;
    }
    if (arg3 != 0) {
        goto fail;
    }
    return current;

advance:
    next = index + 1;
    if (table[(s16)next] != sentinel) {
        index = next;
        goto loop;
    }

fail:
    return -1;
}
