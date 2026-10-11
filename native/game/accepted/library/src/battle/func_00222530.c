typedef struct TerminalPosition {
    unsigned char prefix[0x1C];
    short x;
    short y;
    short z;
} TerminalPosition;

float func_001FF7EC(short x, short z);

int func_00222530(TerminalPosition *first, TerminalPosition *second)
{
    int value = (int)func_001FF7EC(first->x, first->z);

    second->y = (short)value;
    first->y = (short)value;
    return value;
}
