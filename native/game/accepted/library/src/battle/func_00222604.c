typedef unsigned char u8;
typedef signed int s32;

s32 func_00045e5c(u8 value, s32 selector);
s32 func_0020BF98(void);
s32 func_0020BFF8(void *record);
s32 func_0020C014(void *record);
s32 func_0020C0E8(void *record);

s32 func_00222604(void *record, s32 incoming_flags)
{
    s32 result;
    s32 flags;
    s32 selector;
    s32 predicate;

    flags = incoming_flags;
    result = 0;
    /* Retail performs this query even when the 0x8000 flag rejects the record. */
    selector = func_0020BF98();
    if (flags & 0x8000) {
        return 0;
    }
    if (func_0020C014(record) != 0 && (flags & 0x400) != 0) {
        result = 1;
    }
    if (func_0020BFF8(record) != 0 && (flags & 0x400) == 0) {
        result = 1;
    }
    result &= 0 - (selector != 0);
    if (result != 0) {
        if (selector == 0xFF) {
            predicate = func_0020C0E8(record) != 0;
        } else {
            predicate = (func_00045e5c(*((u8 *)record + 0x4B), selector & 0xFF) & 0xFF) != 0;
        }
        result &= 0 - predicate;
    }
    return result;
}
