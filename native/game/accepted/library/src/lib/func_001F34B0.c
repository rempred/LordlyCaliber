typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;

extern void *D_801CE8BC;

void func_800712C4(void *pointer);
void func_80093380(void *pointer, s32 size);

void func_001F34B0(void)
{
    u32 index;
    s32 offset;

    index = 0;
    offset = 0;
    do {
        func_800712C4(*(void **)((u8 *)D_801CE8BC + offset));
        index++;
        offset += 0xC;
    } while (index < 0x10);

    index = 0;
    do {
        func_800712C4(*(void **)((u8 *)D_801CE8BC + 0x180 + index * 4));
        index++;
    } while (index < 0xA);

    func_80093380(D_801CE8BC, 0xC0);
}
