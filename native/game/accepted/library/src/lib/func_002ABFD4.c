typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

extern u8 D_8022A981;
extern u8 D_80196AED;
extern s32 D_801CEAB0;
extern void *D_801CE8BC;
extern void *D_801CE8C0;
extern u8 D_801976D8;
extern u16 D_80197B60;
extern u8 D_8022A7F0;
extern u8 D_80190F80[];
extern u16 D_80193BD8[];
extern u8 D_80193BF3[];

void func_801BE71C(void);
void *func_80070F30(s32 size);
void func_80093380(void *pointer, s32 size);
void func_80080998(void *destination, void *source, s32 size);
void func_801C1D90(void);
void func_801C1A94(void);
void func_801C2310(void);
void func_801BC5FC(void);
void func_801BC640(void);
void func_801B7E9C(void);
void func_801C5CE0(void);
void func_801AECA4(u8 selector);
void func_8020B29C(void);

void func_002ABFD4(void)
{
    u8 *snapshot;
    s32 byte_offset;
    s32 index;
    u16 *halfword_cursor;

    if (D_8022A981 != 0) {
        return;
    }

    D_801CEAB0 = D_80196AED;
    func_801BE71C();
    D_801CE8BC = func_80070F30(0x6094);
    func_80093380(D_801CE8BC, 0x6094);
    D_801CE8C0 = (u8 *)D_801CE8BC + 0x57E0;

    if ((D_801976D8 & 8) || (D_80197B60 != 0)) {
        snapshot = func_80070F30(0x298);
        func_80080998(snapshot, D_80190F80, 0x16C);
        index = 0;
        byte_offset = 0;
        halfword_cursor = (u16 *)snapshot;
        do {
            u8 *row = snapshot + index;
            index++;
            halfword_cursor[0xB6] =
                *(u16 *)((u8 *)D_80193BD8 + byte_offset);
            row[0x234] =
                *(u8 *)((u8 *)D_80193BF3 + byte_offset);
            byte_offset += 0x38;
            halfword_cursor++;
        } while (index < 100);
        *(void **)((u8 *)D_801CE8BC + 0x607C) = snapshot;
    }

    func_801C1D90();
    func_801C1A94();
    func_801C2310();
    func_801BC5FC();
    func_801BC640();
    func_801B7E9C();
    func_801C5CE0();
    func_801AECA4(D_8022A7F0);
    func_8020B29C();
}
