typedef struct {
    int words[20];
} Func0020F3D4Block;

extern unsigned char *D_801CE8BC;
extern void func_001F114C(void *object);

void func_0020F3D4(void)
{
    int offset;
    unsigned int index;
    unsigned int clearIndex;
    unsigned char *object;
    unsigned char *finalBase;
    unsigned char *clearChild;
    unsigned char *child;
    unsigned char *other;
    unsigned char **cursor;
    unsigned char **clearCursor;
    unsigned int value;
    int count;

    index = 0;
    offset = 0;
    do {
        object = 0;
        if (index < 20U) {
            value = (unsigned int)D_801CE8BC;
            if (*(int *)((unsigned char *)value + offset + 0x20C) != 0) {
                object = (unsigned char *)value + (offset + 0x1C4);
            }
        }
        if (object != 0) {
            func_001F114C(object);
            cursor = (unsigned char **)object;
            do {
                child = *cursor;
                if (child != 0) {
                    other = cursor[3];
                    if (other != 0) {
                        *(Func0020F3D4Block *)(other + 0x44) =
                            *(Func0020F3D4Block *)(child + 0x44);
                    }
                    child = *cursor;
                    if (child != 0) {
                        value = child[0x36];
                        if (value < child[0x37]) {
                            child[0x36] = value + 1;
                        }
                    }
                }
                cursor++;
            } while (cursor < (unsigned char **)(object + 0xC));

            child = ((unsigned char **)object)[6];
            if (child != 0) {
                value = child[0x36];
                if (value < child[0x37]) {
                    child[0x36] = value + 1;
                }
            }
            child = ((unsigned char **)object)[7];
            if (child != 0) {
                value = child[0x36];
                if (value < child[0x37]) {
                    child[0x36] = value + 1;
                }
            }

            count = *(int *)(object + 0xA0);
            if (count != 0) {
                count--;
                *(int *)(object + 0xA0) = count;
                if (count == 0) {
                    clearIndex = 0;
                    clearCursor = (unsigned char **)object;
                    do {
                        clearChild = *clearCursor;
                        clearIndex++;
                        if (clearChild != 0) {
                            *(short *)(clearChild + 0x32) = 0;
                            *(short *)(clearChild + 0x30) = 0;
                            *(short *)(clearChild + 0x2E) = 0;
                        }
                        clearCursor++;
                    } while (clearIndex < 3U);
                }
            }

            child = ((unsigned char **)object)[6];
            if (child != 0 && *(short *)(child + 0x48) == 0) {
                *(int *)(child + 0x18) = 0;
                ((unsigned char **)object)[6] = 0;
            }
            child = ((unsigned char **)object)[7];
            if (child != 0 && *(short *)(child + 0x48) == 0) {
                *(int *)(child + 0x18) = 0;
                ((unsigned char **)object)[7] = 0;
            }
        }
        index++;
        offset += 0xF8;
    } while ((int)index < 20);

    finalBase = D_801CE8BC;
    (*(int *)(finalBase + 0x6080))++;
}
