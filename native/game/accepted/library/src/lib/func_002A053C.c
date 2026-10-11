/* Retail callers supply a full word; this field retains its low halfword. */
void func_002A053C(int value)
{
    *(short *)0x800E9C0C = (short)value;
}
