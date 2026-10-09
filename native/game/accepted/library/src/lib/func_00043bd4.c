typedef unsigned char u8;

u8 func_00043bd4(int arg0, int arg1)
{
    int alternate_class;
    int class_index;
    u8 related_class;

    related_class = *(u8 *)((arg0 & 0xFF) * 0x48 + 0x80187C59);
    alternate_class = arg1 & 0xFF;
    if (related_class != alternate_class) {
        class_index = alternate_class * 9;
    } else {
        class_index = related_class * 9;
    }

    return *(u8 *)(class_index * 8 + 0x80187C4D);
}
