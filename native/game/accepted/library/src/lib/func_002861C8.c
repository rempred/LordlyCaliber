typedef signed int s32;
typedef unsigned int u32;

static s32 func_002861C8_scan(s32 *stream, s32 cursor, s32 nesting, s32 mode);

s32 func_002861C8(s32 *nesting, s32 cursor, s32 *stream, s32 value,
                  s32 skip, s32 scan_mode)
{
    s32 matched;
    s32 advance;

    *nesting += 1;
    matched = 0;

    switch ((u32)stream[cursor + 1]) {
    case 0:
        if (value == stream[cursor + 2]) {
            matched = 1;
        }
        break;
    case 1:
        if (value != stream[cursor + 2]) {
            matched = 1;
        }
        break;
    case 2:
        if (value >= stream[cursor + 2]) {
            matched = 1;
        }
        break;
    case 3:
        if (stream[cursor + 2] >= value) {
            matched = 1;
        }
        break;
    case 4:
        if (stream[cursor + 2] < value) {
            matched = 1;
        }
        break;
    case 5:
        if (value < stream[cursor + 2]) {
            matched = 1;
        }
        break;
    }

    if (matched != 0) {
        advance = skip + 2;
        return cursor + advance;
    }
    {
        s32 result;

        result = func_002861C8_scan(stream, cursor + 2, *nesting, scan_mode);
        *nesting -= 1;
        return result;
    }
}

static s32 func_002861C8_scan(s32 *stream, s32 cursor, s32 nesting, s32 mode)
{
    s32 *outer;
    s32 outer_marker;
    s32 limit;
    s32 backward_marker;
    s32 offset;
    s32 current;
    s32 found;

    found = 0;
    if (mode == 1) {
        current = cursor;
        if (current <= 0x0FFFFFFE) {
            s32 *address;

            nesting = (s32)0x80000002;
            cursor = 0x0FFFFFFE;
            address = (s32 *)(current * 4 + (s32)stream);
            stream = address;
loop_forward:
            if (*stream != nesting) {
                current += 1;
                stream += 1;
                if (current > cursor) {
                    return -1;
                }
                goto loop_forward;
            }
            return current;
        }
        return -1;
    }

    current = cursor;
    if (current <= 0x0FFFFFFE) {
        outer_marker = (s32)0x80000000;
        limit = (s32)0x80000002;
        backward_marker = 0x0FFFFFFE;
        outer = (s32 *)(current * 4 + (s32)stream);
        do {
            offset = current * 4;
            if (*outer != outer_marker) {
                goto next_outer;
            }
            if (cursor < current) {
                s32 *address;

                address = (s32 *)(offset + (s32)stream);
                stream = address;
                while (1) {
                    if ((*stream == limit) && (++found, nesting == found)) {
                        return current;
                    }
                    current -= 1;
                    stream -= 1;
                    if (cursor >= current) {
                        return -1;
                    }
                }
            }
            return -1;
next_outer:
            current += 1;
            outer += 1;
            if (current > backward_marker) {
                return -1;
            }
        } while (1);
    }
    return -1;
}

static s32 func_002861C8_find(s32 value, s32 *stream)
{
    s32 cursor;

    cursor = 0;
    do {
        if ((stream[0] == (s32)0x80000005) && (stream[1] == value)) {
            return cursor;
        }
        cursor += 1;
        stream += 1;
    } while (cursor <= 0x0FFFFFFE);
    return 0;
}
