typedef unsigned char u8;

int func_00043e18(int arg0, int arg1)
{
    int primary_class;

    primary_class = arg0 & 0xFF;
    if ((*(u8 *)(primary_class * 6 + 0x8018ECE4) & 0x80) != 0) {
        if (*(u8 *)(primary_class * 0x48 + 0x80187C59) == (arg1 & 0xFF)) {
            arg1 = arg0;
        }
    }

    return *(u8 *)((arg1 & 0xFF) * 6 + 0x8018ECE4) & 0x3F;
}
