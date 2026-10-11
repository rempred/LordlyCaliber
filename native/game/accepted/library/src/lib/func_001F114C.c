extern int func_0020BFF8(void *object);
extern int func_0020C034(void *object);
extern void func_001F1218(void *object, int arg1, int arg2, int arg3, int arg4);

void func_001F114C(void *object)
{
    int *cursor;
    unsigned int index;
    int flag;
    int child;

    index = 0;
    cursor = object;
    do {
        if (*cursor != 0) {
            flag = func_0020BFF8(object);
            func_001F1218((void *)(*cursor + 0x44),
                         ((int *)object)[18], ((int *)object)[19],
                         flag, func_0020C034(object));
        }
        index++;
        cursor++;
    } while (index < 3);

    child = ((int *)object)[6];
    if (child != 0) {
        func_001F1218((void *)(child + 0x44),
                     ((int *)object)[18], ((int *)object)[19], 0, 0);
    }
    child = ((int *)object)[7];
    if (child != 0) {
        func_001F1218((void *)(child + 0x44),
                     ((int *)object)[18], ((int *)object)[19], 0, 0);
    }
}
