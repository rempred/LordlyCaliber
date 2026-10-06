#include "story_cast.h"
static unsigned family(unsigned id) {
    if (id >= 0x51 && id <= 0x53)
        return 0x51;
    if (id >= 0x54 && id <= 0x55)
        return 0x54;
    if (id >= 0x56 && id <= 0x57)
        return 0x56;
    if (id >= 0x5f && id <= 0x60)
        return 0x5f;
    if (id >= 0x63 && id <= 0x64)
        return 0x63;
    return id;
}
void lordly_story_cast(const uint8_t roster[5600], uint8_t pairs[10]) {
    static const uint8_t priority[] = {0x56, 0x54, 0x61, 0x58, 0x5b, 0x5c, 0x59, 0x5a, 0x5e, 0x77,
                                       0x7a, 0x78, 0x75, 0x7b, 0x5d, 0x79, 0x5f, 0x76, 0x7c};
    unsigned count = 0, p, id, fallback = 4;
    for (p = 0; p < sizeof(priority) && count < 5; ++p) {
        for (id = 0; id < 100; ++id) {
            const uint8_t *r = roster + 56 * id;
            if (family(r[17]) != priority[p])
                continue;
            /* The event checks the first matching record. An unavailable first
             * match excludes this family even if a later duplicate is alive. */
            switch (r[18]) {
            case 0x24:
            case 0x25:
            case 0x26:
            case 0x2b:
            case 0x2c:
            case 0x2d:
            case 0x2e:
            case 0x2f:
            case 0x30:
                break;
            default:
                if ((r[24] || r[25]) && !(r[51] & 4)) {
                    pairs[count * 2] = r[17];
                    pairs[count * 2 + 1] = r[18];
                    ++count;
                }
            }
            break;
        }
    }
    while (count < 5) {
        pairs[count * 2] = pairs[count * 2 + 1] = (uint8_t)fallback;
        ++count;
        fallback = fallback == 4 ? 2 : 4;
    }
}
