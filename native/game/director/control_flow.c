/* PC adaptation of the recovered func_002861C8.c in this directory.
 * Array indices replace 32-bit pointer casts. A caller-supplied word limit
 * prevents malformed editor inputs from reading arbitrary host memory.
 * Scanning deliberately remains word-wise, including operand words. */
#include "control_flow.h"

static int scan(const LordlyDirectorWord *stream, int count, int cursor,
                int nesting, int mode, int *result)
{
    int current, found;
    if (mode == 1) {
        for (current = cursor; current < count; ++current) {
            if (stream[current] == 0x80000002U) {
                *result = current;
                return 0;
            }
        }
        return 1;
    }
    for (current = cursor; current < count; ++current) {
        if (stream[current] != 0x80000000U) continue;
        found = 0;
        while (current > cursor) {
            if (stream[current] == 0x80000002U && ++found == nesting) {
                *result = current;
                return 0;
            }
            --current;
        }
        *result = -1;
        return 0;
    }
    return 1;
}
int lordly_director_compare(const LordlyDirectorWord *stream, int count, int cursor,
                           int value, int skip, int scan_mode, int *nesting, int *result)
{
    int matched, target, depth, status, next;
    if (!stream || !nesting || !result || count < 3 || cursor < 0 || cursor > count - 3 ||
        skip < 0 || skip > 1 || scan_mode < 0 || scan_mode > 1 ||
        *nesting < 0 || *nesting >= 0x0fffffff) return 2;
    target = (LordlyDirectorSigned)stream[cursor + 2];
    matched = 0;
    switch (stream[cursor + 1]) {
    case 0: matched = value == target; break;
    case 1: matched = value != target; break;
    case 2: matched = value >= target; break;
    case 3: matched = target >= value; break;
    case 4: matched = target < value; break;
    case 5: matched = value < target; break;
    }
    depth = *nesting + 1;
    if (matched) {
        if (cursor > count - 3 - skip) return 1;
        *result = cursor + skip + 2;
        *nesting = depth;
        return 0;
    }
    status = scan(stream, count, cursor + 2, depth, scan_mode, &next);
    if (status) return status; /* A failed host read does not partially mutate state. */
    *result = next;
    return 0; /* Recovered code increments and then decrements nesting on failure. */
}
int lordly_director_find_label(const LordlyDirectorWord *stream, int count,
                              int value, int *result)
{
    int cursor;
    if (!stream || !result || count < 0) return 2;
    for (cursor = 0; cursor < count; ++cursor) {
        if (stream[cursor] != 0x80000005U) continue;
        if (cursor + 1 == count) return 1;
        if ((LordlyDirectorSigned)stream[cursor + 1] == value) {
            *result = cursor;
            return 0;
        }
    }
    return 1;
}
