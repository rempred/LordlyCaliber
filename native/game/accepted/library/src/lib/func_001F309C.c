typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

extern u8 D_80196AED;
extern s32 D_801CEAB0;
extern void *D_801CE8BC;
extern void *D_801CE8C0;
extern u8 D_801976DA;
extern u8 D_801976D9;
extern u8 D_801976DC;
extern u8 D_801976E8;
extern u8 D_80197207[];
extern u8 D_80190F80[];
extern u16 D_80193BD8[];
extern u8 D_80193BF3[];

s32 func_0020BF7C(void);
s32 func_0020BF8C(void);
void func_00201BAC(void);
void *func_80070F30(s32 size);
void func_80093380(void *pointer, s32 size);
void func_80080998(void *destination, void *source, s32 size);
void func_00205220(void);
void func_00204F24(void);
void func_002057A0(void);
void func_00205760(s32 selector);
void func_001FFA8C(void);
void func_001FFAD0(void);
void func_001FB32C(void);
void func_001F1F20(void *owner);
void func_0020DB10(s32 selector);
void func_00200480(void);
void func_0025FBE0(void);
void func_0020F0BC(void);
void func_00201108(void);
void func_00209774(s32 selector, void *state, u8 value);
void func_0020BD5C(s32 selector);

void func_001F309C(void)
{
    u8 *snapshot;
    s32 byte_offset;
    s32 index;
    u16 *halfword_cursor;
    u8 *owner;
    s32 *state;

    D_801CEAB0 = D_80196AED;
    if (func_0020BF7C() != 0) {
        D_801976DA = 0x18;
        D_801976D9 = 1;
        D_801CEAB0 = 0x31;
    }
    if (func_0020BF8C() != 0) {
        D_801976D9 = 1;
    }

    func_00201BAC();
    D_801CE8BC = func_80070F30(0x6094);
    func_80093380(D_801CE8BC, 0x6094);
    D_801CE8C0 = (u8 *)D_801CE8BC + 0x57E0;
    *(u8 *)((u8 *)D_801CE8BC + 0x6084) = 1;

    if ((func_0020BF7C() != 0) || (func_0020BF8C() != 0)) {
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

    func_00205220();
    func_00204F24();
    func_002057A0();
    func_00205760(2);
    func_001FFA8C();
    func_001FFAD0();
    func_001FB32C();

    owner = D_801CE8BC;
    *(s32 *)(owner + 0x1C0) = D_80197207[D_801976DC * 25];
    func_001F1F20(owner);

    state = D_801CE8C0;
    state[0] = 0;
    state[1] = 0;
    state[2] = 0;
    func_0020DB10(0);
    func_00200480();
    func_0025FBE0();
    func_0020F0BC();
    func_00201108();
    func_00209774(0, (u8 *)D_801CE8C0 + 0x82F, D_801976DC);
    func_00209774(1, (u8 *)D_801CE8C0 + 0x848, D_801976E8);
    func_0020BD5C(2);
}
