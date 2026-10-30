#include "common/types.h"

void func_000509f4(u8 *input, u8 *output)
{
    u8 *start = output;
    u8 value;
    int character;

    while ((value = *input) != 0xFF) {
        if (value < 0x30) {
            if (output - start >= 8) {
                break;
            }
            *output++ = '{';
            *output++ = 'T';
            *output++ = (*input + 1) / 10 + '0';
            *output++ = (*input + 1) % 10 + '0';
            *output = '}';
        } else {
            if (output - start >= 13) {
                break;
            }
            character = value - 0x10;
            if (character == '{' || character >= 0x7F) {
                *output = '.';
            } else {
                *output = value - 0x10;
            }
        }
        input++;
        output++;
    }
    *output = 0;
}
