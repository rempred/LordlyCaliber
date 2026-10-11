#include "dialogue_prefix.h"
static void half(uint8_t *state, int at, int value) {
    const uint16_t word = (uint16_t)value;
    state[at] = (uint8_t)(word >> 8);
    state[at + 1] = (uint8_t)word;
}
int lordly_dialogue_prefix(uint8_t window[168], uint8_t state[1144], const uint8_t *source,
                           size_t length, uint32_t *cursor) {
    uint32_t at = *cursor;
    for (;;) {
        uint8_t command;
        unsigned width = 2;
        int number = 0;
        if (at >= length)
            return -1;
        if (source[at] != '@')
            return 0;
        if (length - at < 2)
            return -1;
        command = source[at + 1];
        switch (command) {
        case '*':
        case 'A':
        case '$':
        case '-':
        case 'b':
        case 'x':
        case 'k':
        case 'g':
        case 'u':
        case 'm':
        case 'r':
        case '^':
            break;
        case 'S':
        case 'l':
            width = 3;
            break;
        case 'f':
        case 'F':
        case 'v':
        case 'y':
        case '<':
            width = 5;
            break;
        default:
            return 0;
        }
        if (length - at < width)
            return -1;
        if (width == 5)
            number = ((int)source[at + 2] - 48) * 100 + ((int)source[at + 3] - 48) * 10 +
                     (int)source[at + 4] - 48;
        switch (command) {
        case '*':
            state[0x47] = 1;
            break;
        case 'A':
            state[0x51] = 1;
            break;
        case '$':
            state[0x47] = 2;
            break;
        case '-':
            window[0x91] |= 4;
            break;
        case 'S':
            state[0x48] |= (uint8_t)(1U << (((unsigned)source[at + 2] - 48U) & 31U));
            break;
        case 'b':
            window[0x91] |= 1;
            break;
        case 'x':
            window[0x8a] |= 0x80;
            break;
        case 'k':
            state[0x41] = 1;
            break;
        case 'g':
            window[0x8a] ^= 0x20;
            break;
        case 'u':
            window[0x8a] |= 8;
            break;
        case 'm':
            window[0x8a] |= 0x10;
            break;
        case 'f':
        case 'F':
            state[0x4a] = command == 'F';
            half(state, 0x14, number);
            break;
        case 'l':
            state[0x4b] = (uint8_t)(source[at + 2] - 48);
            break;
        case 'v':
            half(state, 0x1a, number);
            break;
        case 'y':
            half(state, 0x1e, number);
            break;
        case 'r':
            state[0x4e] = 1;
            break;
        case '^':
            state[0x52] = 1;
            break;
        case '<':
            window[4] = (uint8_t)number;
            break;
        }
        at += width;
        *cursor = at;
    }
}
