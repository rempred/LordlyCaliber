/* Command-stream dispatcher, ROM 00284288..002861C8.
* Numeric commands and field offsets retain uncertainty in their meanings.
* Cursor updates after mode-dependent blocks are deliberately shared: KMC
* merges their tails before filling the retail branch and call delay slots.
* The two scalar control flags are separate locals, not a synthetic stack record.
*/
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
void func_00019e10();
void func_00019fc0();
void func_0001a050(void);
void func_00045cb0(s8);
s32 func_00045df4(s16);
void func_000466f4();
void func_00048024();
void func_00048294();
void func_0004ecfc();
void func_001F0E04(s32, s32);
void func_0026285C(s32, u16);
void func_0028308C(s32, s32, s32);
void func_002831E0(s32, s32);
void func_00283224(s32, s32, s32, s32);
void func_002834C4(s32, s32);
void func_00283564(s32, s32, s32, s32);
void func_00283654(void);
s32 func_00283694(void);
void func_002836A4(void);
s32 func_002836C8(s32);
void func_00283740(void);
void func_00283748(s32);
void func_002838AC(s32, s32, s32, s32, s32, s32);
void func_002839A8();
s32 func_002839C4();
void func_00283B30(void);
s32 func_00283E14(s32);
void func_00283FA8(s32, s32);
s32 func_002861C8(s32 *nesting, s32 cursor, s32 *stream, s32 value, s32 skip, s32 scan_mode);
s32 func_002864F8(s32);
void func_0029A4C0(void *);
void func_0029A59C(s32, s32, s32, s32);
void func_0029A7C4(u8, s32, s32, s32, s32, s32, s32, s32, s32);
void func_0029AC44(s32);
void func_0029AF24();
void func_0029B78C(void *);
void func_0029B9E0(s32);
void func_0029BA80(s32, s32, s32, s32, s32, s32, s32, s32, s32);
s32 func_0029BDC4(s32);
void func_0029BDF8(s32);
void func_0029BEF4(s32, void *);
s32 func_0029C19C(void);
void func_0029C1B8(void *, void *, s32, s32);
void func_0029C5A0(s32, s32);
void func_0029C624(s32, s32, s32, s32, s32);
s32 func_0029C92C(s32);
void func_0029D790(s32, s32, s32, s32, f32, f32, f32, u8, u8);
void func_0029DEC8(s32, f32, s32);
s32 func_0029DF04(s32);
s32 func_0029FF74(s32);
void func_002A0088(s32, s32, s32, s32, s32, s32, s32, s32, s32, s32);
s32 func_002A0354(s32);
void func_002A0374(s32);
void func_002A053C(s32);
void func_002A0548(s32);
u8 func_002A05DC(s32);
void func_002A05EC(s32);
s32 func_002A08C0(s32, s32, s32, s32, f32, f32, f32, s32, s32);
void func_002A0B14(s32, f32, f32, f32, f32, s32, s32);
s32 func_002A13C4(s32);
void func_002A1610(s32, f32, f32, f32);
void func_002A1690(s32, s32);
void func_002A175C(s32, s32);
void func_002A2C78();
void func_002A44EC(u8, u16);
void func_002A6150(s32, s32);
s32 func_002A7010(s32, f32, f32, f32);
void func_002a3bc0(s32, s32, s32, s32);
s32 func_002a3efc(s32);
s32 func_8022A428(s32, s32 *);
void func_00289D04(void);
void func_00289D24();
s32 func_00289E88();
void func_0028b088();
void func_0028B218();
void func_0028B264();
void func_0028B348();
void func_0028F918(u8);
void func_002907E8(s32);
s32 func_002907F8(void);
void func_00290810(s32);
void func_00290C50(s32, s32, s32, s32, s32, s32);
s32 func_002910DC();
void func_002AE4DC();
void func_002A91E4(f32, f32, f32, f32, s32);
s32 func_002A92C8();
void func_002A9364(s32, s32, s32, s32, s32, s32, s32, s32);
s32 func_002A98C4(s32);
void func_002AF3EC(s32, s32);
void func_002AF470(s16, s16, s16, s16, s32, s32);
void func_002AF5C4();
void func_002AF6C0(void *);
void func_002AAC60(s32);
void func_002AB2A8(s16);
s16 func_002AB4DC();
void func_00297CC8(s32, s32);
void func_002AB508(s32);
void func_002B0D30(s32, s32);
void func_002B0E8C(s32, s32, s32);
s32 func_002B1014(void);
void func_002B1028(s32, s32, s32);
void func_002ABB3C(s32, s32, s32);
s32 func_002ABF64(s32);
void func_002ABFD4(s32);
void func_00298914(s32, s32);
void func_002B19D8(s32);
s32 func_002AC700(s32);
void func_002AC84C(s32, s32);
void func_002AC948(s32, s32);
void func_002B20C4(s32, s32);
void func_002ACF08(s32);
void func_002ACFD8(s32);
s32 func_002AD420(void);
void func_002AD434(s32, s32);
void func_00299CA8(u8, s32);
void func_002AD6D0(s32, s32, s32, s32, s32, s32);
s32 func_002ADA20();
void func_002ADA80();
void func_002B3494(s32, f32, f32, f32, f32, f32, s32, s32);
void func_002B3798(u8, u8, u8);
s32 func_002B38A0();
void func_002B4104();
void func_002B4990(s32);
void func_002B4B88();
void func_002B4D58(s32, s32, s32, s32, s32, s32, s32);
s32 func_002B4FA0(void);
void func_002B56A8(s32, s32);
void func_002B578C(s32, s32, s32);
s32 func_002B57C4(s32);
void func_002B5AA8(s32, s32, s32, s32, s32, s32, s32, s32, s32);
s32 func_002B5D58();
void func_002B64AC(s32, s32, s32, s32, s32, s32, s32, s32, s32);
void func_002B6CD0(u8, s32, s32, s32, s32, s32, s32, s32, s32, s32);
void func_002B6E30(u8, s32, s32, s32);
s32 func_002B6EDC(s32);
void func_002B88C8(s16, s16);
void resource_free(void *);
extern u16 D_800E8100;
extern u16 D_8018F1A0;
extern void * D_8018F1A4;
extern u8 D_8018FC19;
extern u8 D_801936A9;
extern u16 D_80196A2C;
extern s32 D_801CEAB0;
extern s8 D_801CFC70;
extern s32 D_801D06F4;
extern f32 D_801D0718;
extern f32 D_801D0768;
extern s32 D_801D07D0;
extern s32 D_801D07D8;
extern f32 D_801D0814;
extern s32 D_8022A950;
extern s32 D_8022A954;
extern s32 * D_8022A958;
extern s32 * D_8022A95C;
extern s8 D_8022A960;
extern void * D_8022A970;
extern void * D_8022A974;
extern s32 D_8022A990;
extern s16 D_8022A994;
extern s8 D_8022A998;
extern u8 D_8022A99A;
extern s8 D_8022AC80;
extern void * D_8023DE34;

s32 func_00284288(void) {
    u16 code_choices[2];
    s32 *stream;
    s32 nesting;
    s32 stop_requested;
    s32 state_scan_mode;
    s32 variant_83_8b;
    s32 variant_45_ab;
    s16 args_3d;
    s16 value_bb;
    s32 command_result;
    s32 args_3f;
    s32 args_6b;
    s32 args_6c;
    s32 args_88;
    s32 args_8c;
    s32 args_9b;
    s32 args_a7;
    s32 value_aa;
    s32 next_stream_selector;
    s32 args_5;
    s32 args_6;
    s32 args_13;
    s32 args_1a;
    s32 args_29;
    void *value_33_2;
    s32 value_62;
    s32 value_33;
    void *value_33_4;
    s32 value_47;
    s32 value_80000009;
    s32 *previous_stream;
    s32 command;
    s32 first_45_ab;
    s32 args_59;
    s32 value_bf;
    s32 cursor_byte_offset;
    s32 second_45_ab;
    s32 cursor;
    s32 value_bf_2;
    s32 scan_mode;
    u8 value_83_8b;
    u8 stream_selector;
    void *args_bf;
    void *command_address;
    void *args_3b;
    void *args_3b_2;
    void *args_45;
    void *value_47_3;
    void *args_48;
    void *args_4a;
    void *args_4b;
    void *args_50;
    void *args_5f;
    void *args_64;
    void *args_69;
    void *args_6e;
    void *args_6f;
    void *args_70;
    void *args_71;
    void *args_7b;
    void *args_9c;
    void *args_9d;
    void *args_a6;
    void *args_ab;
    void *args_ae;
    void *value_b2;
    void *value_b3;
    void *args_b4;
    void *args_c2;
    void *args_80000009;
    void *args_5e;
    void *args_3;
    void *args_1b;
    void *args_26;
    void *args_2a;
    void *args_2c;
    void *value_33_3;
    void *args_3a;
    void *args_46;
    void *value_47_2;
    void *args_49;
    void *args_4d;
    void *args_62;
    void *args_63;
    void *args_76;
    void *value_80000000;
    void *args_7;
    void *args_14;
    void *args_1e;
    stream = D_8022A958;
    stream_selector = 0xFF;
    scan_mode = 0;
    cursor = D_8022A950;
    stop_requested = 0;
    state_scan_mode = 0;
    nesting = 0;
    for (; cursor <= 0x0FFFFFFE; cursor++) {
        cursor_byte_offset = cursor * 4;
        command_address = (u8 *)(cursor_byte_offset + (s32)stream);
        command = *(s32 *)((u8 *)command_address + 0);
        command_result = -0x64;
        switch (command) {
        default: break;
        case 0x0:
            stop_requested = 1;
            break;
        case 0x1:
            func_00283654();
            break;
        case 0x2:
            func_002836A4();
            break;
        case 0x3:
            { f32 x, y, z;
                args_3 = (s32 *)(cursor * 4 + (s32)stream);
                x = (f32)*(s32 *)((s8 *)args_3 + 0x14) / 1000.0f;
                y = (f32)*(s32 *)((s8 *)args_3 + 0x18) / 1000.0f;
                z = (f32)*(s32 *)((s8 *)args_3 + 0x1C) / 1000.0f;
                func_002A08C0(
                *(s32 *)((s8 *)args_3 + 4),
                *(s32 *)((s8 *)args_3 + 8),
                *(s32 *)((s8 *)args_3 + 0xC),
                *(s32 *)((s8 *)args_3 + 0x10),
                x,
                y,
                z,
                *(s32 *)((s8 *)args_3 + 0x20),
                0);
                cursor += 8;
                break;
            }
        case 0x5:
            args_5 = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_002A0548(args_5);
            break;
        case 0x6:
            args_6 = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_002A0374(args_6);
            break;
        case 0x7:
            args_7 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 7;
            func_002A0B14(
                *(s32 *)((u8 *)args_7 + 4),
                (f32) *(s32 *)((u8 *)args_7 + 8) / 1000.0f,
                (f32) *(s32 *)((u8 *)args_7 + 0xC) / 1000.0f,
                (f32) *(s32 *)((u8 *)args_7 + 0x10) / 1000.0f,
                (f32) *(s32 *)((u8 *)args_7 + 0x14) / 1000.0f,
                *(s32 *)((u8 *)args_7 + 0x18),
                *(s32 *)((u8 *)args_7 + 0x1C));
            break;
        case 0x8:
            if (D_8018FC19 == 0) {
                func_002B1028(
                    *(s32 *)((u8 *)command_address + 4),
                    *(s32 *)((u8 *)command_address + 8),
                    *(s32 *)((u8 *)command_address + 12));
            }
            cursor += 3;
            break;
        case 0xD:
            scan_mode = 0;
            if (D_8018FC19 != 0) {
                break;
            }
            command_result = func_002B1014();
            break;
        case 0xF:
            scan_mode = 1;
            command_result = func_0029DF04(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)));
            break;
        case 0x10:
            scan_mode = 1;
            command_result = func_002A0354(*(s32 *)((u8 *)command_address + 0xC));
            break;
        case 0x11:
            scan_mode = 1;
            command_result = func_002A13C4(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)));
            break;
        case 0x12:
            break;
        case 0x13:
            args_13 = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_002A05EC(args_13);
            break;
        case 0x14:
            args_14 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 9;
            func_0029D790(
                *(s32 *)((u8 *)args_14 + 4),
                *(s32 *)((u8 *)args_14 + 8),
                *(s32 *)((u8 *)args_14 + 0xC),
                *(s32 *)((u8 *)args_14 + 0x10),
                (f32) *(s32 *)((u8 *)args_14 + 0x14) / 1000.0f,
                (f32) *(s32 *)((u8 *)args_14 + 0x18) / 1000.0f,
                (f32) *(s32 *)((u8 *)args_14 + 0x1C) / 1000.0f,
                (s32) *(u8 *)((u8 *)args_14 + 0x23),
                (s32) *(u8 *)((u8 *)args_14 + 0x27));
            break;
        case 0x15:
            cursor += 5;
            func_0029C624(
                *(s32 *)((u8 *)command_address + 4),
                *(s32 *)((u8 *)command_address + 8),
                *(s32 *)((u8 *)command_address + 0xC),
                *(s32 *)((u8 *)command_address + 0x10),
                *(s32 *)((u8 *)command_address + 0x14));
            break;
        case 0x16:
            scan_mode = 1;
            command_result = func_0029C92C(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)));
            break;
        case 0x1A:
            args_1a = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_002A053C(args_1a);
            break;
        case 0x1B:
            args_1b = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 6;
            func_002838AC(
                *(s32 *)((u8 *)args_1b + 4),
                *(s32 *)((u8 *)args_1b + 8),
                *(s32 *)((u8 *)args_1b + 0xC),
                *(s32 *)((u8 *)args_1b + 0x10),
                *(s32 *)((u8 *)args_1b + 0x14),
                *(s32 *)((u8 *)args_1b + 0x18));
            break;
        case 0x1C:
            if (D_8018FC19 == 0) {
                s32 *args = (s32 *)(cursor * 4 + (s32)stream);
                func_002AF3EC(args[1], args[2]);
            }
            cursor += 2;
            break;
        case 0x1D:
            cursor += 2;
            func_002A1690(*(s32 *)((u8 *)command_address + 4), *(s32 *)((u8 *)command_address + 8));
            break;
        case 0x1E:
            args_1e = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 9;
            func_0029BA80(
                *(s32 *)((u8 *)args_1e + 4),
                *(s32 *)((u8 *)args_1e + 8),
                *(s32 *)((u8 *)args_1e + 0xC),
                *(s32 *)((u8 *)args_1e + 0x10),
                *(s32 *)((u8 *)args_1e + 0x14),
                *(s32 *)((u8 *)args_1e + 0x18),
                *(s32 *)((u8 *)args_1e + 0x1C),
                *(s32 *)((u8 *)args_1e + 0x20),
                *(s32 *)((u8 *)args_1e + 0x24));
            break;
        case 0x1F:
            scan_mode = 1;
            command_result = func_0029BDC4(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)));
            break;
            break;
        case 0x20:
            break;
        case 0x21:
            cursor += 3;
            break;
        case 0x22:
            cursor += 4;
            func_002A1610(
                *(s32 *)((u8 *)command_address + 4),
                (f32) *(s32 *)((u8 *)command_address + 8) / 1000.0f,
                (f32) *(s32 *)((u8 *)command_address + 0xC) / 1000.0f,
                (f32) *(s32 *)((u8 *)command_address + 0x10) / 1000.0f);
            break;
        case 0x25:
            scan_mode = 1;
            command_result = func_002A05DC((*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)))) & 0xFF;
            break;
        case 0x26:
            args_26 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 4;
            func_002a3bc0(
                *(s32 *)((u8 *)args_26 + 4),
                *(s32 *)((u8 *)args_26 + 8),
                *(s32 *)((u8 *)args_26 + 0xC),
                *(s32 *)((u8 *)args_26 + 0x10));
            break;
        case 0x27:
            scan_mode = 1;
            command_result = func_002a3efc(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)));
            break;
        case 0x28:
            func_0029B78C((u8 *)stream + (cursor_byte_offset + 4));
            cursor += 8;
            break;
        case 0x29:
            args_29 = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_0029B9E0(args_29);
            break;
        case 0x2A:
            if (D_8018FC19 == 2) {
                args_2a = (s32 *)(cursor * 4 + (s32)stream);
                cursor += 6;
                func_002A9364(
                    *(s32 *)((u8 *)args_2a + 4),
                    *(s32 *)((u8 *)args_2a + 8),
                    *(s32 *)((u8 *)args_2a + 0x18),
                    *(s32 *)((u8 *)args_2a + 0xC),
                    *(s32 *)((u8 *)args_2a + 0x10),
                    *(s32 *)((u8 *)args_2a + 0x14),
                    0,
                    0);
            } else {
                cursor += 6;
            }
            break;
        case 0x2B:
            if (D_8018FC19 != 2) {
                break;
            }
            scan_mode = 1;
            command_result = func_002A98C4(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            break;
        case 0x2C:
            if (D_8018FC19 == 2) {
                args_2c = (s32 *)(cursor * 4 + (s32)stream);
                func_002A91E4(
                    (f32) *(s32 *)((u8 *)args_2c + 4) / 1000.0f,
                    (f32) *(s32 *)((u8 *)args_2c + 8) / 1000.0f,
                    (f32) *(s32 *)((u8 *)args_2c + 0xC) / 1000.0f,
                    (f32) *(s32 *)((u8 *)args_2c + 0x10) / 1000.0f,
                    *(s32 *)((u8 *)args_2c + 0x14));
            }
            cursor += 5;
            break;
        case 0x2D:
            if (D_8018FC19 != 2) {
                break;
            }
            command_result = func_002A92C8();
            scan_mode = 0;
            break;
        case 0x30:
            command_result = func_0029C19C();
            scan_mode = 0;
            break;
        case 0x33:
            { s32 arg2, arg3;
                value_33 = cursor * 4;
                value_33_2 = (u8 *)stream + (value_33 + 4);
                value_33_3 = (u8 *)(value_33 + (s32)stream);
                arg2 = *(s32 *)((u8 *)value_33_3 + 0x1C);
                arg3 = *(s32 *)((u8 *)value_33_3 + 0x20);
                value_33_4 = (u8 *)stream + (value_33 + 0x10);
                func_0029C1B8(value_33_2, value_33_4, arg2, arg3);
                cursor += 8;
                break;
            }
        case 0x35:
            func_002AF6C0((u8 *)stream + (cursor * 4 + 4));
            cursor += 7;
            break;
        case 0x36:
            func_0029A4C0((u8 *)stream + (cursor * 4 + 4));
            cursor += 7;
            break;
        case 0x39:
            cursor += 1;
            func_002AB508(*(s32 *)((u8 *)command_address + 4));
            break;
        case 0x3A:
            args_3a = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 8;
            func_002B3494(
                *(s32 *)((u8 *)args_3a + 4),
                (f32) *(s32 *)((u8 *)args_3a + 8),
                (f32) *(s32 *)((u8 *)args_3a + 0xC),
                (f32) *(s32 *)((u8 *)args_3a + 0x10),
                (f32) *(s32 *)((u8 *)args_3a + 0x14),
                (f32) *(s32 *)((u8 *)args_3a + 0x18),
                *(s32 *)((u8 *)args_3a + 0x1C),
                *(s32 *)((u8 *)args_3a + 0x20));
            break;
        case 0x3B:
            if (D_8022A994 != 0) {
                goto case_3b_join_305;
            }
            args_3b = (u8 *)((cursor * 4) + (s32)stream);
            cursor += 6;
            func_00290C50(
                *(s32 *)((u8 *)args_3b + 4),
                *(s32 *)((u8 *)args_3b + 8),
                *(s32 *)((u8 *)args_3b + 0xC),
                *(s32 *)((u8 *)args_3b + 0x10),
                *(s32 *)((u8 *)args_3b + 0x14),
                *(s32 *)((u8 *)args_3b + 0x18));
            break;
        case_3b_join_305:
            args_3b_2 = (u8 *)((cursor * 4) + (s32)stream);
            cursor += 6;
            func_002AD6D0(
                *(s32 *)((u8 *)args_3b_2 + 4),
                *(s32 *)((u8 *)args_3b_2 + 8),
                *(s32 *)((u8 *)args_3b_2 + 0xC),
                *(s32 *)((u8 *)args_3b_2 + 0x10),
                *(s32 *)((u8 *)args_3b_2 + 0x14),
                *(s32 *)((u8 *)args_3b_2 + 0x18));
            break;
        case 0x3C:
            if (D_8022A994 != 0) {
                goto case_3c_join_308;
            }
            command_result = func_002910DC() & 0xFF;
            scan_mode = 0;
            break;
        case_3c_join_308:
            command_result = func_002ADA20();
            scan_mode = 0;
            break;
        case 0x3D:
            args_3d = (*(s16 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (6)));
            cursor += 1;
            func_002AB2A8(args_3d);
            break;
        case 0x3E:
            command_result = func_002AB4DC();
            scan_mode = 0;
            break;
        case 0x3F:
            args_3f = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_002AAC60(args_3f);
            break;
        case 0x40:
            command_result = func_002AD420();
            scan_mode = 0;
            break;
        case 0x45:
            args_45 = (s32 *)(cursor * 4 + (s32)stream);
            first_45_ab = *(s32 *)((u8 *)args_45 + 4);
            second_45_ab = *(s32 *)((u8 *)args_45 + 8);
            variant_45_ab = 0;
            goto case_ab_join_408;
        case 0x46:
            args_46 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 9;
            func_0029A7C4(
                *(u8 *)((u8 *)args_46 + 7),
                *(s32 *)((u8 *)args_46 + 8),
                *(s32 *)((u8 *)args_46 + 0xC),
                *(s32 *)((u8 *)args_46 + 0x10),
                *(s32 *)((u8 *)args_46 + 0x14),
                *(s32 *)((u8 *)args_46 + 0x18),
                *(s32 *)((u8 *)args_46 + 0x1C),
                *(s32 *)((u8 *)args_46 + 0x20),
                *(s32 *)((u8 *)args_46 + 0x24));
            break;
        case 0x47:
            if (D_8018FC19 != 2) {
                goto case_47_join_318;
            }
            D_801D0768 = (f32) *(s32 *)((u8 *)command_address + 4);
            D_801D0718 = (f32) *(s32 *)((u8 *)command_address + 8);
            D_801D0814 = (f32) *(s32 *)((u8 *)command_address + 0xC);
        case_47_join_318:
            value_47 = cursor * 4;
            cursor += 3;
            value_47_2 = D_8022A974;
            value_47_3 = (u8 *)(value_47 + (s32)stream);
            *(f32 *)((u8 *)value_47_2 + 4) = (f32) *(s32 *)((u8 *)value_47_3 + 4);
            *(f32 *)((u8 *)value_47_2 + 8) = (f32) *(s32 *)((u8 *)value_47_3 + 8);
            *(f32 *)((u8 *)value_47_2 + 0xC) = (f32) *(s32 *)((u8 *)value_47_3 + 0xC);
            break;
        case 0x48:
            args_48 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 2;
            func_002A175C(*(s32 *)((u8 *)args_48 + 4), *(s32 *)((u8 *)args_48 + 8));
            break;
        case 0x49:
            args_49 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 9;
            func_002B5AA8(
                *(s32 *)((u8 *)args_49 + 4),
                *(s32 *)((u8 *)args_49 + 8),
                *(s32 *)((u8 *)args_49 + 0xC),
                *(s32 *)((u8 *)args_49 + 0x10),
                *(s32 *)((u8 *)args_49 + 0x14),
                *(s32 *)((u8 *)args_49 + 0x18),
                *(s32 *)((u8 *)args_49 + 0x1C),
                *(s32 *)((u8 *)args_49 + 0x20),
                *(s32 *)((u8 *)args_49 + 0x24));
            break;
        case 0x4A:
            args_4a = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 3;
            func_002B578C(
                *(s32 *)((u8 *)args_4a + 4),
                *(s32 *)((u8 *)args_4a + 8),
                *(s32 *)((u8 *)args_4a + 0xC));
            break;
        case 0x4B:
            args_4b = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 2;
            func_002B56A8(*(s32 *)((u8 *)args_4b + 4), *(s32 *)((u8 *)args_4b + 8));
            break;
        case 0x4C:
            scan_mode = 1;
            command_result = func_002B57C4(*(s32 *)((u8 *)command_address + 0xC));
            break;
        case 0x4D:
            args_4d = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 9;
            func_002B64AC(
                *(s32 *)((u8 *)args_4d + 4),
                *(s32 *)((u8 *)args_4d + 8),
                *(s32 *)((u8 *)args_4d + 0xC),
                *(s32 *)((u8 *)args_4d + 0x10),
                *(s32 *)((u8 *)args_4d + 0x14),
                *(s32 *)((u8 *)args_4d + 0x18),
                *(s32 *)((u8 *)args_4d + 0x1C),
                *(s32 *)((u8 *)args_4d + 0x20),
                *(s32 *)((u8 *)args_4d + 0x24));
            break;
        case 0x4E:
            command_result = func_002B5D58();
            scan_mode = 0;
            break;
        case 0x50:
            args_50 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 3;
            func_002B3798(
                *(u8 *)((u8 *)args_50 + 7),
                *(u8 *)((u8 *)args_50 + 0xB),
                *(u8 *)((u8 *)args_50 + 0xF));
            break;
        case 0x54:
            command_result = func_002B38A0();
            scan_mode = 0;
            break;
        case 0x55:
            func_002B4104();
            break;
        case 0x56:
            state_scan_mode = 1;
            break;
        case 0x57:
            scan_mode = 1;
            command_result = func_002864F8(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)));
            break;
        case 0x59:
            args_59 = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            D_8022A950 = func_8022A428(args_59, stream);
            return 0;
        case 0x5C:
            cursor += 1;
            break;
        case 0x5D:
            /* Indexed call inputs let KMC hoist them above the fixed-global byte store. */
            D_8022A960 = (s8) *(s32 *)((u8 *)command_address + 0xC);
            cursor += 3;
            func_002B0D30(((s32 *)command_address)[3], ((s32 *)command_address)[1]);
            func_002AE4DC();
            break;
        case 0x5E:
            args_5e = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 3;
            func_002B0E8C(
                *(s32 *)((u8 *)args_5e + 4),
                *(s32 *)((u8 *)args_5e + 8),
                *(s32 *)((u8 *)args_5e + 0xC));
            break;
        case 0x5F:
            args_5f = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 6;
            func_002AF470(
                *(s16 *)((u8 *)args_5f + 6),
                *(s16 *)((u8 *)args_5f + 0xA),
                *(s16 *)((u8 *)args_5f + 0xE),
                *(s16 *)((u8 *)args_5f + 0x12),
                (s32) *(s16 *)((u8 *)args_5f + 0x16),
                (s32)*(s16 *)((s8 *)args_5f + 0x1A));
            break;
        case 0x60:
            { s32 *args = (s32 *)(cursor * 4 + (s32)stream);
                s32 index = args[1];
                s32 value = args[2];
                func_001F0E04(D_801D07D0 + index * 0x64 + 0x14, value);
                cursor += 2;
            }
            break;
        case 0x61:
            cursor += 2;
            func_001F0E04(
                *(s32 *)((*(s32 *)((u8 *)command_address + 4) * 0xF8) + D_801D07D8) + 0x44,
                *(s32 *)((u8 *)command_address + 8));
            break;
        case 0x62:
            args_62 = (s32 *)(cursor * 4 + (s32)stream);
            value_62 = *(s32 *)((u8 *)args_62 + 8);
            if (value_62 < 4) {
                goto case_62_join_340;
            }
            if ((u32) (value_62 - 0x19) >= 0xAU) {
                goto case_62_join_341;
            }
        case_62_join_340:
            cursor += 2;
            func_002A6150(*(s32 *)((u8 *)args_62 + 4), value_62);
            break;
        case_62_join_341:
            cursor += 2;
            func_00297CC8(*(s32 *)((u8 *)args_62 + 4), value_62);
            break;
        case 0x63:
            args_63 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 0xA;
            func_002B6CD0(
                *(u8 *)((u8 *)args_63 + 7),
                *(s32 *)((u8 *)args_63 + 8),
                *(s32 *)((u8 *)args_63 + 0xC),
                *(s32 *)((u8 *)args_63 + 0x10),
                *(s32 *)((u8 *)args_63 + 0x14),
                *(s32 *)((u8 *)args_63 + 0x18),
                *(s32 *)((u8 *)args_63 + 0x1C),
                *(s32 *)((u8 *)args_63 + 0x20),
                *(s32 *)((u8 *)args_63 + 0x24),
                *(s32 *)((u8 *)args_63 + 0x28));
            break;
        case 0x64:
            args_64 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 4;
            func_002B6E30(
                *(u8 *)((u8 *)args_64 + 7),
                *(s32 *)((u8 *)args_64 + 8),
                *(s32 *)((u8 *)args_64 + 0xC),
                *(s32 *)((u8 *)args_64 + 0x10));
            break;
        case 0x65:
            scan_mode = 1;
            command_result = func_002B6EDC(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)));
            break;
        case 0x66:
            cursor += 1;
            func_0029AC44(*(s32 *)((u8 *)command_address + 4));
            break;
        case 0x69:
            args_69 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 3;
            func_0029DEC8(
                *(s32 *)((u8 *)args_69 + 4),
                (f32) *(s32 *)((u8 *)args_69 + 8) / 1000.0f,
                *(s32 *)((u8 *)args_69 + 0xC));
            break;
        case 0x6A:
            scan_mode = 1;
            func_00283740();
            command_result = func_002836C8(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)));
            break;
        case 0x6B:
            args_6b = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_00283748(args_6b);
            break;
        case 0x6C:
            args_6c = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_002B4990(args_6c);
            break;
        case 0x6D:
            func_002B4B88();
            break;
        case 0x6E:
            args_6e = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 3;
            func_0028308C(
                *(s32 *)((u8 *)args_6e + 4),
                *(s32 *)((u8 *)args_6e + 8),
                *(s32 *)((u8 *)args_6e + 0xC));
            break;
        case 0x6F:
            args_6f = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 2;
            func_002831E0(*(s32 *)((u8 *)args_6f + 4), *(s32 *)((u8 *)args_6f + 8));
            break;
        case 0x70:
            args_70 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 2;
            func_002834C4(*(s32 *)((u8 *)args_70 + 4), *(s32 *)((u8 *)args_70 + 8));
            break;
        case 0x71:
            args_71 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 4;
            func_00283564(
                *(s32 *)((u8 *)args_71 + 4),
                *(s32 *)((u8 *)args_71 + 8),
                *(s32 *)((u8 *)args_71 + 0xC),
                *(s32 *)((u8 *)args_71 + 0x10));
            break;
        case 0x73:
            func_0029AF24();
            break;
        case 0x75:
            func_002AF5C4();
            break;
        case 0x76:
            args_76 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 7;
            func_002B4D58(
                *(s32 *)((u8 *)args_76 + 4),
                *(s32 *)((u8 *)args_76 + 8),
                *(s32 *)((u8 *)args_76 + 0xC),
                *(s32 *)((u8 *)args_76 + 0x10),
                *(s32 *)((u8 *)args_76 + 0x14),
                *(s32 *)((u8 *)args_76 + 0x18),
                *(s32 *)((u8 *)args_76 + 0x1C));
            break;
        case 0x77:
            command_result = func_002B4FA0();
            scan_mode = 0;
            break;
        case 0x7B:
            args_7b = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 4;
            func_0029A59C(
                *(s32 *)((u8 *)args_7b + 4),
                *(s32 *)((u8 *)args_7b + 8),
                *(s32 *)((u8 *)args_7b + 0xC),
                *(s32 *)((u8 *)args_7b + 0x10));
            break;
        case 0x7C:
            cursor += 1;
            *(s8 *)((u8 *)D_8022A974 + 0x1CB0) = (s8) *(s32 *)((u8 *)command_address + 4);
            break;
        case 0x7D:
            func_002839A8();
            D_8022A998 = 1;
            return 0;
        case 0x7E:
            func_002839A8();
            break;
        case 0x7F:
            command_result = func_002839C4();
            scan_mode = 0;
            break;
        case 0x80:
            { void *record = *(void **)((u8 *)D_8022A974 + 0x1A48);
                u8 value = *(u8 *)(cursor * 4 + (s32)stream + 7);
                cursor++;
                *(s16 *)((u8 *)record + 4) = value;
                func_002A2C78();
            }
            break;
        case 0x82:
            func_0028B264();
            goto case_b8_join_429;
        case 0x83:
            value_83_8b = (*(u8 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (7)));
            variant_83_8b = 0;
            goto case_8b_join_373;
        case 0x84:
            func_00019e10();
            func_00019fc0();
            break;
        case 0x85:
            func_0001a050();
            break;
        case 0x86:
            func_0028B348();
            *(s8 *)((u8 *)D_8023DE34 + 0x66) = 0;
            break;
        case 0x87:
            command_result = func_002907F8();
            scan_mode = 0;
            break;
        case 0x88:
            args_88 = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_002907E8(args_88);
            break;
        case 0x8B:
            value_83_8b = (*(u8 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (7)));
            variant_83_8b = 1;
        case_8b_join_373:
            cursor += 1;
            func_00299CA8(value_83_8b, variant_83_8b);
            break;
        case 0x8C:
            args_8c = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_00290810(args_8c);
            break;
        case 0x8D:
            cursor += 1;
            break;
        case 0x8F:
            scan_mode = 1;
            command_result = func_00045df4(
                (s16) ((*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC))) + 0xAA));
            break;
        case 0x90:
            scan_mode = 1;
            command_result = func_002AC700(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)));
            break;
        case 0x92:
            if (D_8018FC19 == 2) {
                s32 *args = (s32 *)(cursor * 4 + (s32)stream);
                func_002AC84C(args[1], args[2]);
            }
            cursor += 2;
            break;
        case 0x94:
            cursor += 1;
            if (D_8018FC19 != 2) {
                break;
            }
            func_0028F918(*(u8 *)((u8 *)command_address + 7));
            break;
        case 0x95:
            if (D_8018FC19 == 2) {
                s32 *args = (s32 *)(cursor * 4 + (s32)stream);
                func_002AC948(args[1], args[2] - 1);
            }
            /* Fall through: this command consumes the same two-word tail. */
        case 0x8E:
            cursor += 2;
            break;
        case 0x96:
            if (D_8018FC19 == 2) {
                func_002ACF08(*(s32 *)(cursor * 4 + (s32)stream + 4));
            }
            cursor++;
            break;
        case 0x97:
            func_00283B30();
            break;
        case 0x99:
            stream_selector = (*(u8 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (7)));
            cursor += 1;
            break;
        case 0x9A:
            stream_selector = 0xFE;
            break;
        case 0x9B:
            args_9b = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_002B19D8(args_9b);
            break;
        case 0x9C:
            args_9c = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 2;
            func_002B20C4(*(s32 *)((u8 *)args_9c + 4), *(s32 *)((u8 *)args_9c + 8));
            break;
        case 0x9D:
            args_9d = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 2;
            func_0029C5A0(*(s32 *)((u8 *)args_9d + 4), *(s32 *)((u8 *)args_9d + 8));
            break;
        case 0xA1:
            scan_mode = 1;
            command_result = func_002ABF64(*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (0xC)));
            break;
        case 0xA4:
            cursor += 1;
            func_002ACFD8(*(s32 *)((u8 *)command_address + 4));
            break;
        case 0xA5:
            command_result = func_002AD420();
            scan_mode = 0;
            break;
        case 0xA6:
            args_a6 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 2;
            func_002AD434(*(s32 *)((u8 *)args_a6 + 4), *(s32 *)((u8 *)args_a6 + 8));
            break;
        case 0xA7:
            args_a7 = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (4)));
            cursor += 1;
            func_0029BDF8(args_a7);
            break;
        case 0xA9:
            func_002ADA80();
            break;
        case 0xAA:
            code_choices[0] = 0x1009;
            code_choices[1] = 0x1005;
            value_aa = *(s32 *)((u8 *)D_8022A974 + 0);
            if (value_aa != 0) {
                func_0026285C(value_aa, code_choices[*(s32 *)((s8 *)(command_address) + (4))]);
            }
            cursor += 1;
            break;
        case 0xAB:
            args_ab = (s32 *)(cursor * 4 + (s32)stream);
            first_45_ab = *(s32 *)((u8 *)args_ab + 4);
            second_45_ab = *(s32 *)((u8 *)args_ab + 8);
            variant_45_ab = 1;
        case_ab_join_408:
            cursor += 2;
            func_002ABB3C(first_45_ab, second_45_ab, variant_45_ab);
            break;
        case 0xAE:
            args_ae = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 2;
            func_00298914(*(s32 *)((u8 *)args_ae + 4), *(s32 *)((u8 *)args_ae + 8));
            break;
        case 0xAF:
            func_00289D04();
            break;
        case 0xB0:
            func_00289D24();
            break;
        case 0xB1:
            command_result = func_00289E88() & 0xFF;
            scan_mode = 0;
            break;
        case 0xB2:
            value_b2 = D_8022A970;
            if (value_b2 == 0) {
                goto case_b2_join_415;
            }
            *(u8 *)((u8 *)value_b2 + 0x840) = 1U;
            break;
        case_b2_join_415:
            break;
        case 0xB3:
            value_b3 = D_8022A970;
            if (value_b3 == 0) {
                goto case_b3_join_419;
            }
            *(u8 *)((u8 *)value_b3 + 0x840) = 0U;
            break;
        case_b3_join_419:
            break;
        case 0xB4:
            args_b4 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 4;
            func_00283224(
                *(s32 *)((u8 *)args_b4 + 4),
                *(s32 *)((u8 *)args_b4 + 8),
                *(s32 *)((u8 *)args_b4 + 0xC),
                *(s32 *)((u8 *)args_b4 + 0x10));
            break;
        case 0xB5:
            if (!(D_800E8100 & 0x8000)) {
                goto case_e_join_424;
            }
            D_8022A990 = (*(s32 *)((s8 *)(((s32 *)(cursor * 4 + (s32)stream))) + (8))) + 1;
        case 0xE:
        case_e_join_424:
            command_result = func_00283694();
            scan_mode = 0;
            break;
        case 0xB6:
            if (D_8018F1A4 == 0) {
                func_00045cb0((s8) *(u8 *)((u8 *)command_address + 7));
            }
            cursor += 1;
            break;
        case 0xB7:
            func_0028b088();
            *(s8 *)((u8 *)D_8023DE34 + 0x66) = 2;
            break;
        case 0xB8:
            func_0028B218();
        case_b8_join_429:
            *(s8 *)((u8 *)D_8023DE34 + 0x66) = 1;
            break;
        case 0xBB:
            { s32 next_id;
                func_00048294();
                func_00048024();
                func_000466f4();
                value_bb = D_8018F1A0 & 0x3FFF;
                D_8022A994 = value_bb;
                D_8022AC80 = func_00283E14(value_bb);
                next_id = (s32) D_8022A994;
                resource_free(D_8022A958);
                D_8022A958 = 0;
                func_00283FA8(next_id, 0);
                return 0;
            }
        case 0xBC:
            scan_mode = 0;
            goto case_bc_join_432;
        case_bc_join_432:
            { u32 range_value = D_80196A2C - 6;
                command_result = range_value < 0xCU; }
            break;
        case 0xBD:
            scan_mode = 0;
            command_result = D_8018F1A4 != 0;
            break;
        case 0xBE:
            command_result = (D_801936A9 / 34) & 0xFF;
            if (command_result < 3) {
                goto case_be_join_436;
            }
            command_result = 2;
            scan_mode = 0;
            break;
        case_be_join_436:
            scan_mode = 0;
            break;
        case 0xBF:
            args_bf = (s32 *)(cursor * 4 + (s32)stream);
            value_bf = *(s32 *)((u8 *)args_bf + 0x2C);
            value_bf_2 = 0;
            if (value_bf != -1) {
                goto case_bf_join_440;
            }
            value_bf = func_0029FF74(*(s32 *)((u8 *)args_bf + 0x10));
        case_bf_join_440:
            if (*(s32 *)((u8 *)args_bf + 0x30) != 1) {
                goto case_bf_join_442;
            }
            value_bf = 0 - value_bf;
            goto case_bf_join_444;
        case_bf_join_442:
        case_bf_join_444:
            if (*(s32 *)((u8 *)args_bf + 0x24) != 1) {
                goto case_bf_join_446;
            }
            value_bf_2 = 1;
            goto case_bf_join_448;
        case_bf_join_446:
        case_bf_join_448:
            if (*(s32 *)((u8 *)args_bf + 0x28) != 1) {
                goto case_bf_join_450;
            }
            value_bf_2 |= 2;
            goto case_bf_join_452;
        case_bf_join_450:
        case_bf_join_452:
            cursor += 0xD;
            func_002A0088(
                *(s32 *)((u8 *)args_bf + 4),
                *(s32 *)((u8 *)args_bf + 0xC),
                *(s32 *)((u8 *)args_bf + 0x14),
                *(s32 *)((u8 *)args_bf + 0x18),
                *(s32 *)((u8 *)args_bf + 0x20),
                *(s32 *)((u8 *)args_bf + 0x10),
                value_bf_2 & 0xFF,
                *(s32 *)((u8 *)args_bf + 8),
                *(s32 *)((u8 *)args_bf + 0x34),
                value_bf);
            break;
        case 0xC2:
            args_c2 = (s32 *)(cursor * 4 + (s32)stream);
            cursor += 2;
            func_002B88C8(*(s16 *)((u8 *)args_c2 + 6), *(s16 *)((u8 *)args_c2 + 0xA));
            break;
        case 0xC3:
            { u8 i = 0;
                u8 *row = (u8 *)(D_801D06F4 + D_801CEAB0 * 6);
                do {
                    row[i] = 0;
                    i++;
                } while (i < 6);
            }
            break;
        case 0x80000000:
            next_stream_selector = stream_selector & 0xFF;
            if (stop_requested != 1) {
                goto case_80000000_join_459;
            }
            D_8022A950 = (s32) (cursor + 1);
        case_80000000_join_459:
            if (next_stream_selector == 0xFE) {
                goto case_80000000_join_462;
            }
            if (next_stream_selector == 0xFF) {
                goto case_80000000_join_463;
            }
            func_00283FA8(next_stream_selector, 1);
            cursor = -1;
            stream = D_8022A958;
            stream_selector = 0xFF;
            continue;
        case_80000000_join_462:
            resource_free(D_8022A958);
            { s32 previous_cursor = D_8022A954;
                previous_stream = D_8022A95C;
                D_8022A958 = 0;
                D_8022A950 = 0;
                D_8022A95C = 0;
                D_8022A950 = previous_cursor;
                D_8022A958 = previous_stream;
            }
        case_80000000_join_463:
            value_80000000 = D_8022A970;
            if (value_80000000 == 0) {
                return 0;
            }
            if (*(u8 *)((u8 *)value_80000000 + 0x840) != 2) {
                return 0;
            }
            *(u8 *)((u8 *)value_80000000 + 0x840) = 0U;
            D_8022A950 = func_8022A428(0x3039, stream);
            return 0;
        case 0x80000001:
            return -1;
            /* Its default behavior also supplies a node in KMC's balanced switch tree. */
        case 0x80000002:
            break;
        case 0x80000003:
            { s32 next_id = *(s32 *)((u8 *)command_address + 4);
                resource_free(D_8022A958);
                D_8022A958 = 0;
                func_00283FA8(next_id, 0);
                return 0;
            }
        case 0x80000004:
            cursor += 1;
            break;
        case 0x80000005:
            if (stream[cursor + 1] == 0x70) {
                u8 value = D_8022A99A;
                if (value >= 3) {
                    D_8022A99A = value - 1;
                }
            }
            cursor++;
            break;
        case 0x80000006:
            if (D_8018FC19 == 2) {
                func_002ABFD4(*(s32 *)(cursor * 4 + (s32)stream + 4));
            } else {
                s32 *args = (s32 *)(cursor * 4 + (s32)stream);
                D_8022A960 = (s8)args[1];
                func_002B0D30(args[1], 0);
                func_002AE4DC();
            }
            cursor++;
            break;
        case 0x80000007:
            cursor += 1;
            break;
        case 0x80000008:
            cursor += 2;
            func_002A44EC(*(u8 *)((u8 *)command_address + 7), *(u16 *)((u8 *)command_address + 0xA));
            break;
        case 0x80000009:
            if (D_801CFC70 != -1) {
                goto case_80000009_join_477;
            }
            func_0004ecfc();
        case_80000009_join_477:
            args_80000009 = (u8 *)((cursor * 4) + (s32)stream);
            cursor += 4;
            value_80000009 = func_002A7010(
                *(s32 *)((u8 *)args_80000009 + 4),
                (f32) *(s32 *)((u8 *)args_80000009 + 8) / 1000.0f,
                (f32) *(s32 *)((u8 *)args_80000009 + 0xC) / 1000.0f,
                (f32) *(s32 *)((u8 *)args_80000009 + 0x10) / 1000.0f);
            D_801CFC70 = 2;
            *(s32 *)((u8 *)D_8022A974 + 0) = value_80000009;
            break;
        case 0x8000000A:
            if (D_801CFC70 != -1) {
                goto case_8000000a_join_480;
            }
            func_0004ecfc();
        case_8000000a_join_480:
            func_0029BEF4(*(s32 *)(cursor * 4 + (s32)stream + 4),
            (u8 *)stream + (cursor * 4 + 8));
            cursor += 5;
        }
        if (command_result != -0x64) {
            cursor = func_002861C8(&nesting, cursor, stream, command_result, scan_mode, state_scan_mode);
        }
    }
    return 0;
}
