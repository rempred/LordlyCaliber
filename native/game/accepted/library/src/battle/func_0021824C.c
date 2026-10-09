typedef signed int s32;
typedef unsigned char u8;
typedef unsigned short u16;

s32 func_0020BFF8(s32);
s32 func_0020C24C(s32);
s32 func_0020C478(s32);

void func_0021824C(s32 *out0, s32 *out1, s32 *out2, s32 *out3)
{
    u8 *state;
    u8 mode;
    u16 flags;
    s32 i;
    s32 item;

    *out0 = 1;
    *out1 = 1;
    *out2 = 1;
    *out3 = 0;

    state = *(u8 **)0x801CE8BC;
    mode = state[0x6087];
    if (mode == 0) {
        flags = *(u16 *)(state + 0x606A);
        if (flags != 0) {
            if (flags & 0x2000) {
                if (flags & 0x0400) {
                    *out1 = 0;
                    *out0 = 2;
                } else {
                    *out1 = 2;
                    *out0 = 0;
                }
            } else if (flags & 0x4000) {
                if (flags & 0x0400) {
                    *out1 = 0;
                    *out0 = 1;
                } else {
                    *out1 = 1;
                    *out0 = 0;
                }
            } else if (flags & 0x1000) {
                if (flags & 0x0400) {
                    *out1 = 0;
                    *out0 = 3;
                } else {
                    *out1 = 3;
                    *out0 = 0;
                }
            } else if (flags & 0x8000) {
                if (flags & 0x0400) {
                    *out1 = 0;
                    *out0 = 3;
                } else {
                    *out1 = 3;
                    *out0 = 0;
                }
            }

            if ((*(u16 *)(*(u8 **)0x801CE8BC + 0x606A) & 0x0800) == 0) {
                *out2 = 0;
            }
            *out3 = 1;
        }
    } else if (mode == 1) {
        *out0 = 1;
        *out1 = 0;
        *out2 = 1;
    } else if (mode == 2) {
        *out0 = 0;
        *out1 = 1;
        *out2 = 1;
    } else if (mode == 3) {
        *out0 = 0;
        *out1 = 0;
        *out2 = 1;
    }

    i = 0;
    do {
        item = func_0020C478(i);
        if (func_0020BFF8(item) != 0) {
            if (func_0020C24C(item) != 0) {
                if (*(u16 *)(*(u8 **)0x801CE8BC + 0x606A) == 0) {
                    *out1 = 0;
                    break;
                }
            }
            i++;
        } else {
            i++;
        }
    } while (i < 20);
}
