extern char *D_8022A974;

void func_000016C4(void *ptr);

void func_002A05EC(int slot)
{
    int end;
    int index;

    if (slot == -1) {
        slot = 0;
        end = 28;
    } else {
        end = slot + 1;
    }
    index = slot;

    for (; index < end; index++) {
        int offset = index * 4;

        if (*(void **)(offset + (unsigned int)D_8022A974 + 0x18) != 0) {
            func_000016C4(*(void **)(offset + (unsigned int)D_8022A974 + 0x18));
            *(void **)(offset + (unsigned int)D_8022A974 + 0x18) = 0;
        }
    }
}
