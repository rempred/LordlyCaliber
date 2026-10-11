typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

extern void *D_801CE8BC;
extern u8 D_80190F80[];
extern u16 D_80193BD8[];
extern u8 D_80193BF3[];

s32 func_0020BF7C(void);
s32 func_0020BF8C(void);
void func_80080998(void *destination, void *source, s32 size);
void func_800712C4(void *pointer);
void func_00201D24(void);
void func_0025FC48(void);
void func_001FF108(void);
void func_80093380(void *pointer, s32 size);
void func_000521fc(void *pointer);
void func_00205228(void);
void func_00204F2C(void);
void func_002057DC(void);
void func_0020F1D8(void);

void func_001F3540(void)
{
    u8 *snapshot;
    s32 index;
    s32 byte_offset;
    u16 *halfword_cursor;
    u8 value;
    u32 cleanup_index;
    s32 cleanup_offset;
    void *pointer;

    if (D_801CE8BC != 0) {
        if ((func_0020BF7C() != 0) || (func_0020BF8C() != 0)) {
            snapshot = *(u8 **)((u8 *)D_801CE8BC + 0x607C);
            func_80080998(D_80190F80, snapshot, 0x16C);

            index = 0;
            halfword_cursor = (u16 *)snapshot;
            byte_offset = 0;
            do {
                *(u16 *)((u8 *)D_80193BD8 + byte_offset) =
                    halfword_cursor[0xB6];
                value = *(u8 *)((u32)snapshot + index + 0x234);
                halfword_cursor++;
                index++;
                *(u8 *)((u8 *)D_80193BF3 + byte_offset) = value;
                byte_offset += 0x38;
            } while (index < 100);

            func_800712C4(*(void **)((u8 *)D_801CE8BC + 0x607C));
            *(void **)((u8 *)D_801CE8BC + 0x607C) = 0;
        }

        cleanup_index = 0;
        func_00201D24();
        cleanup_offset = 0;
        func_0025FC48();
        func_001FF108();

        do {
            func_800712C4(*(void **)((u8 *)D_801CE8BC + cleanup_offset));
            cleanup_index++;
            cleanup_offset += 0xC;
        } while (cleanup_index < 0x10);

        cleanup_index = 0;
        do {
            func_800712C4(*(void **)((u8 *)D_801CE8BC +
                                     0x180 + cleanup_index * 4));
            cleanup_index++;
        } while (cleanup_index < 0xA);

        func_80093380(D_801CE8BC, 0xC0);

        pointer = *(void **)((u8 *)D_801CE8BC + 0x5748);
        if (pointer != 0) {
            func_000521fc(pointer);
        }
        pointer = *(void **)((u8 *)D_801CE8BC + 0x574C);
        if (pointer != 0) {
            func_000521fc(pointer);
        }
        pointer = *(void **)((u8 *)D_801CE8BC + 0x57D4);
        if (pointer != 0) {
            func_000521fc(pointer);
        }

        func_00205228();
        func_00204F2C();
        func_002057DC();
        func_0020F1D8();
        func_800712C4(D_801CE8BC);
        D_801CE8BC = 0;
    }
}
