typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;

typedef struct Func000135A0State {
    u8 pad_00[0x0C];
    s32 field_0C;
    u8 pad_10[0x04];
    s32 field_14;
    u8 pad_18[0x20];
    u8 *field_38;
    u8 pad_3C[0x66];
    u16 field_A2;
    u8 pad_A4[0x18];
    u8 field_BC;
} Func000135A0State;

void func_000135a0(Func000135A0State *arg0)
{
    s32 wait_one;
    u16 wait_count;
    s32 elapsed;
    u8 *cursor_1;
    u8 *cursor_2;
    u8 *cursor_3;
    u8 opcode_byte;
    s32 high_bits;

    wait_one = 1;
    while (1) {
        elapsed = arg0->field_14 + 0x100;
        wait_count = arg0->field_A2 - 1;
        arg0->field_14 = elapsed;
        arg0->field_A2 = wait_count;
        if ((u16)wait_count == 0) {
            cursor_1 = arg0->field_38;
            arg0->field_38 = cursor_1 + 1;
            opcode_byte = *cursor_1;
            if ((u32)opcode_byte >= 0x80) {
                arg0->field_BC = opcode_byte & 0x7F;
                cursor_2 = arg0->field_38;
                arg0->field_38 = cursor_2 + 1;
                opcode_byte = *cursor_2;
                if ((u32)opcode_byte >= 0x80) {
                    high_bits = (opcode_byte & 0x7F) << 8;
                    arg0->field_A2 = high_bits;
                    cursor_3 = arg0->field_38;
                    arg0->field_38 = cursor_3 + 1;
                    opcode_byte = *cursor_3;
                    high_bits = high_bits + 2;
                    arg0->field_A2 = opcode_byte + high_bits;
                } else {
                    arg0->field_A2 = opcode_byte + 2;
                }
            } else {
                arg0->field_BC = opcode_byte;
                arg0->field_A2 = wait_one;
            }
        }
        if (arg0->field_14 - arg0->field_0C >= 0) {
            return;
        }
    }
}
