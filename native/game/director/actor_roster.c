#include "actor_roster.h"
/* ROM45E5C: byte equality followed by the five native appearance families. */
static int same(uint8_t a, uint8_t b) {
    static const uint8_t families[5][2] = {
        {0x51, 0x53}, {0x54, 0x55}, {0x56, 0x57}, {0x5f, 0x60}, {0x63, 0x64}};
    if (a == b)
        return 1;
    for (int i = 0; i < 5; ++i)
        if (a >= families[i][0] && a <= families[i][1] && b >= families[i][0] &&
            b <= families[i][1])
            return 1;
    return 0;
}
int lordly_roster_select(int32_t wanted[2], uint32_t flags, int32_t source) {
    const int deployed = (flags >> 8) & 1, leader = (flags >> 9) & 1;
    const int32_t a = wanted[0], b = wanted[1];
    if (!source)
        return 0;
    if ((a == -1 && deployed) || (b == -1 && !deployed) || (a == -2 && deployed && leader) ||
        (b == -2 && !deployed && leader))
        return 1;
    if ((a == 0 && deployed) || (b == 0 && !deployed))
        return 0;
    if (same((uint8_t)a, (uint8_t)source)) {
        wanted[0] = 0;
        return 1;
    }
    if (same((uint8_t)b, (uint8_t)source)) {
        wanted[1] = 0;
        return 1;
    }
    return 0;
}
int lordly_roster_pose_offset(uint32_t flags, int32_t depth, int context, int scenario,
                              int variant) {
    int flip = depth < 4;
    if (scenario == 0x3d9 || scenario == 0x21f || scenario == 0xc6 || scenario == 0xf5 ||
        scenario == 0x1e4 || scenario == 0x1e5)
        flip = 1 - flip;
    if ((flags & 2) && (!(flags & 0x100) || !(flags & 0x200)))
        return 28;
    if ((context == -9 || context == -6) && flip && variant == 0)
        return 26;
    return -1;
}
int lordly_roster_leader_slot(uint32_t flags, int32_t owner, int pose, int private_primary) {
    if ((owner == 10 || owner == 25 || owner == 123) && pose >= 50 && !private_primary)
        return -1;
    if (!(flags & 0x200))
        return -1;
    return (flags >> 8) & 1;
}
