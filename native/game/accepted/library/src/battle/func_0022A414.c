typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;

s32 func_0020C014(void *record);

void func_0022A414(void *record)
{
    u32 index;
    s32 mode;
    u8 *cursor;
    u8 *entry;

    mode = func_0020C014(record);
    index = 0;
    if (mode != 0) {
        *(s32 *)((u8 *)record + 0x58) += 1;
        *(s32 *)((u8 *)record + 0x5C) += 0x26;
        cursor = record;
        do {
            entry = *(u8 **)cursor;
            index++;
            if (entry != 0) {
                *(u16 *)(entry + 0x1C) += 0x26;
                entry = *(u8 **)cursor;
                *(u16 *)(entry + 0x28) = *(u16 *)(entry + 0x1C);
            }
            cursor += 4;
        } while (index < 3);
    } else {
        *(s32 *)((u8 *)record + 0x58) -= 1;
        *(s32 *)((u8 *)record + 0x5C) -= 0x26;
        cursor = record;
        do {
            entry = *(u8 **)cursor;
            index++;
            if (entry != 0) {
                *(u16 *)(entry + 0x1C) -= 0x26;
                entry = *(u8 **)cursor;
                *(u16 *)(entry + 0x28) = *(u16 *)(entry + 0x1C);
            }
            cursor += 4;
        } while (index < 3);
    }
}
