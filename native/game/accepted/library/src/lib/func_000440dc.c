typedef signed char s8;
typedef unsigned char u8;

void *func_000440dc(int primary_class, int alternate_class)
{
    s8 *primary_record;
    int primary_offset;

    primary_offset = (primary_class & 0xFF) * 0x48;
    primary_record = (s8 *)(primary_offset + 0x80180000);
    alternate_class &= 0xFF;
    if (*(u8 *)(primary_record + 0x7C59) == alternate_class) {
        return *(void **)(primary_record + 0x7C14);
    }

    return *(void **)((alternate_class * 0x48) + 0x80187C14);
}
