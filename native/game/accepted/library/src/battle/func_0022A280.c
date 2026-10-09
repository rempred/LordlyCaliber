typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
s32 func_0020C1B4(void *record);
s32 func_0020C2C0(void *record);
void *func_0020C478(unsigned int index);
void func_0021D230(s32 record, s32 arg1, void *arg2, s32 arg3);
void func_0021D25C(s32 record, s32 arg1, void *arg2, s32 arg3, s32 arg4);
s32 func_002224F4(void);
s32 func_0022257C(void *record, s32 arg1, s32 *arg2);
s32 func_00239AA4(void *record, u16 arg1);
void func_0022A280(void *record, s32 arg1, u8 arg2)
{
    s32 flags;
    s32 result;
    s32 index;
    void *entry;
    s32 duration;
    s32 time;
    s32 firstTime;
    flags = *(s32 *)((u8 *)record + 0x40) & ~0x3E;
    *(u16 *)((u8 *)record + 0x20) = 0;
    *(s32 *)((u8 *)record + 0x40) = flags | 1;
    result = func_002224F4();
    *(u16 *)((u8 *)*(void **)0x801CE8BC + 0x606A) = result;
    index = 0;
    if ((u16)result != 0) {
        do {
            entry = func_0020C478(index);
            result = func_0020C2C0(entry);
            index++;
            if (result == 0) {
                *(s32 *)((u8 *)entry + 0x6C) = *(s32 *)((u8 *)entry + 0x70);
            }
        } while (index < 20);
        if (func_00239AA4(record, *(u16 *)((u8 *)*(void **)0x801CE8BC + 0x606A)) != 0) {
            func_0021D25C(0x1D, arg1, record, 0xFF, 0x24);
            firstTime = arg1 + func_0022257C((u8 *)*(void **)record + 0x44, 0x24, 0);
            if (*(s32 *)((u8 *)record + 0x94) < firstTime) {
                *(s32 *)((u8 *)record + 0x94) = firstTime;
            }
            return;
        }
    }
    duration = func_0022257C((u8 *)*(void **)record + 0x44, 0x1D, 0);
    if (func_0020C1B4(record) != 0 && (arg2 & 0x10)) {
        func_0021D230(0x30, arg1, record, 0xFF);
        func_0021D230(0x31, arg1 + duration, record, 0xFF);
    } else {
        func_0021D230(0x13, arg1, record, 0xFF);
        func_0021D230(0x14, arg1 + duration, record, 0xFF);
    }
    time = arg1 + duration;
    if (*(s32 *)((u8 *)record + 0x94) < time) {
        *(s32 *)((u8 *)record + 0x94) = time;
    }
}
