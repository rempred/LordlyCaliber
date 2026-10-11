typedef unsigned char u8;
typedef signed int s32;

s32 func_0020C014(void *);
s32 func_0020C24C(void *);
s32 func_0020C284(void *);
s32 func_0020C2C0(void *);
void *func_0020D770(s32, s32);

s32 func_0022A4E0(void *record)
{
    s32 forward58, back58;
    s32 field54, field58, result, predicate;
    void *entry;
    result = 1;
    if (func_0020C24C(record) != 0) {
        return 0;
    }
    if (*(u8 *)0x801936E0 != 0) {
        return 0;
    }
    if (func_0020C014(record) != 0) {
        forward58 = *(s32 *)((u8 *)record + 0x58);
        if (forward58 == 8) {
            result = 0;
            goto done;
        }
        if (func_0020C2C0(func_0020D770(*(s32 *)((u8 *)record + 0x54), forward58 + 1)) == 0) {
            result = 0;
            goto done;
        }
        if (func_0020C284(record) != 0) {
            result = func_0020C2C0(func_0020D770(*(s32 *)((u8 *)record + 0x54), *(s32 *)((u8 *)record + 0x58) + 2)) != 0;
            if (func_0020C2C0(func_0020D770(*(s32 *)((u8 *)record + 0x54) + 1, *(s32 *)((u8 *)record + 0x58) + 2)) == 0) result = 0;
            if (func_0020C2C0(func_0020D770(*(s32 *)((u8 *)record + 0x54) - 1, *(s32 *)((u8 *)record + 0x58) + 2)) == 0) result = 0;
            goto done;
        }
        entry = func_0020D770(*(s32 *)((u8 *)record + 0x54), *(s32 *)((u8 *)record + 0x58) + 2);
        if (func_0020C2C0(entry) == 0) {
            result = func_0020C284(entry) == 0;
        }
        entry = func_0020D770(*(s32 *)((u8 *)record + 0x54) + 1, *(s32 *)((u8 *)record + 0x58) + 2);
        if (func_0020C2C0(entry) == 0) {
            result &= 0 - (func_0020C284(entry) == 0);
        }
        field54 = *(s32 *)((u8 *)record + 0x54);
        field58 = *(s32 *)((u8 *)record + 0x58);
        field54--;
        field58 += 2;
        goto final_neighbor;
    }
    back58 = *(s32 *)((u8 *)record + 0x58);
    if (back58 == 0) {
        result = 0;
        goto done;
    }
    if (func_0020C2C0(func_0020D770(*(s32 *)((u8 *)record + 0x54), back58 - 1)) == 0) {
        result = 0;
        goto done;
    }
    if (func_0020C284(record) != 0) {
        result = func_0020C2C0(func_0020D770(*(s32 *)((u8 *)record + 0x54), *(s32 *)((u8 *)record + 0x58) - 2)) != 0;
        if (func_0020C2C0(func_0020D770(*(s32 *)((u8 *)record + 0x54) + 1, *(s32 *)((u8 *)record + 0x58) - 2)) == 0) result = 0;
        if (func_0020C2C0(func_0020D770(*(s32 *)((u8 *)record + 0x54) - 1, *(s32 *)((u8 *)record + 0x58) - 2)) == 0) result = 0;
        goto done;
    }
    entry = func_0020D770(*(s32 *)((u8 *)record + 0x54), *(s32 *)((u8 *)record + 0x58) - 2);
    if (func_0020C2C0(entry) == 0) {
        result = func_0020C284(entry) == 0;
    }
    entry = func_0020D770(*(s32 *)((u8 *)record + 0x54) + 1, *(s32 *)((u8 *)record + 0x58) - 2);
    if (func_0020C2C0(entry) == 0) {
        result &= 0 - (func_0020C284(entry) == 0);
    }
    field54 = *(s32 *)((u8 *)record + 0x54);
    field58 = *(s32 *)((u8 *)record + 0x58);
    field54--;
    field58 -= 2;
final_neighbor:
    entry = func_0020D770(field54, field58);
    if (func_0020C2C0(entry) == 0) {
        predicate = func_0020C284(entry) == 0;
        result = result & (0 - predicate);
    }
done:
    return result;
}
