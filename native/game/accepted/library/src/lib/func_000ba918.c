typedef unsigned char u8;
typedef signed int s32;

s32 func_000ba918(s32 arg0, s32 outer_index, u8 *entry, s32 result)
{
    s32 offset;
    s32 inner_index;
    u8 *base;

    outer_index = 0;
    base = *(u8 **)0x80196AF8;
    arg0 &= 0xFF;
    offset = 0x10D4;
    do {
        entry = base + offset;
        if (entry[8] != 0) {
            inner_index = 0;
            result = outer_index & 0xFF;
            do {
                if ((entry + inner_index)[2] == arg0) {
                    return result;
                }
                inner_index++;
            } while (inner_index < 5);
        }
        outer_index++;
        offset += 0xE;
    } while (outer_index < 0xA);
    return 0xFF;
}
