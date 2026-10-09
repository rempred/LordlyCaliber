typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
s32 func_0020C1B4(void *record);
s32 func_0020C2C0(void *record);
s32 func_0020C32C(void *record);
void *func_0020C478(u32 index);
void func_0021D784(void *record);
void func_0021D7A8(void *record);
void func_0021D7CC(void *record);
s32 func_002224F4(void);

void func_0022B06C(void *record, u8 *state)
{
    s32 result;
    s32 index;
    s32 flags;
    if (func_0020C2C0(record) != 0 && *state != 7) {
        *state = 0;
        return;
    }
    switch (*state) {
    case 3:
        if (func_0020C32C(record) != 0) {
            *state = 0;
            break;
        }
        *(s32 *)((u8 *)record + 0x40) |= 2;
        result = func_002224F4();
        *(u16 *)((u8 *)*(void **)0x801CE8BC + 0x606A) = result;
        index = 0;
        if ((u16)result != 0) {
            for (;;) {
                record = func_0020C478(index);
                result = func_0020C2C0(record);
                index++;
                if (result == 0) {
                    *(s32 *)((u8 *)record + 0x6C) = *(s32 *)((u8 *)record + 0x70);
                }
                if (index >= 20) {
                    break;
                }
            }
        }
        break;
    case 8:
        flags = *(s32 *)((u8 *)record + 0x40);
        if (flags & 0xA) {
            *state = 0;
            break;
        }
        *(s32 *)((u8 *)record + 0x40) = flags | 8;
        break;
    case 4:
        flags = *(s32 *)((u8 *)record + 0x40);
        if (flags & 0xE) {
            *state = 0;
            break;
        }
        *(s32 *)((u8 *)record + 0x40) = flags | 4;
        break;
    case 10:
        flags = *(s32 *)((u8 *)record + 0x40);
        if (flags & 0x1E) {
            *state = 0;
            break;
        }
        result = flags | 0x10;
        goto store_flags;
    case 5:
        func_0021D784(record);
        break;
    case 6:
        func_0021D7A8(record);
        break;
    case 11:
        flags = *(s32 *)((u8 *)record + 0x40);
        if (!(flags & 0x3E)) {
            goto clear;
        }
        result = flags & ~0x3E;
store_flags:
        *(s32 *)((u8 *)record + 0x40) = result;
        func_0021D7CC(record);
        break;
    case 7:
        if (func_0020C1B4(record) != 0) {
            break;
        }
        /* Otherwise share the state reset with case 2. */
    case 2:
clear:
        *state = 0;
        break;
    }
}
