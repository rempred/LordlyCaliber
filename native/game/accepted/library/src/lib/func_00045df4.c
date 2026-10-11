typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;

int func_00045df4(s16 arg0)
{
    s16 magnitude;
    int byte_index;
    int result;
    int was_negative;

    magnitude = arg0;
    was_negative = 0;
    if (arg0 < 0) {
        magnitude = -arg0;
        was_negative = 1;
    }

    byte_index = magnitude / 8;
    if (((*(u8 *)((s8 *)(byte_index + 0x80190000) + 0x3686)) >>
         (magnitude - (byte_index * 8))) & 1) {
        result = 1 - was_negative;
        goto done;
    }

    result = was_negative;

done:
    return result;
}
