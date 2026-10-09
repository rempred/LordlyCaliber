typedef unsigned char u8;
typedef signed int s32;

s32 func_000ba8a8(s32 arg0, s32 outer_index, u8 *entry, s32 result)
{
    s32 offset;
    s32 inner_index;
    s32 sentinel;
    u8 *base;

    outer_index = 0;
    base = *(u8 **)0x80196AF8;
    sentinel = 0xFF;
    arg0 &= 0xFF;
    offset = 0x117C;
    do {
        entry = base + offset;
        if (entry[3] != sentinel) {
            inner_index = 0;
            result = outer_index & 0xFF;
            do {
                if ((entry + inner_index)[4] == arg0) {
                    return result;
                }
                inner_index++;
            } while (inner_index < 9);
        }
        outer_index++;
        offset += 0x36;
    } while (outer_index < 0x1E);
    return 0xFF;
}
