typedef unsigned char u8;
typedef signed int s32;

extern void func_00023780(void *destination, s32 bytes);

void func_00131388(void *record)
{
    u8 buffer[10];
    u8 *write_cursor;
    u8 *read_cursor;
    s32 index;

    /* Preserve nonzero entries in order and zero the remaining field bytes. */
    func_00023780(buffer, 10);
    index = 0;
    write_cursor = buffer;
    do {
        u8 value = ((u8 *)record + index)[13];
        ++index;
        if (value != 0) {
            *write_cursor = value;
            ++write_cursor;
        }
    } while (index < 10);

    index = 0;
    read_cursor = buffer;
    do {
        u8 value = *read_cursor;
        ((u8 *)record + index)[13] = value;
        ++index;
        ++read_cursor;
    } while (index < 10);
}
