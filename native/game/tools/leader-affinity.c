/* Custom combat group lookup. The native owner keeps leader selection and
 * eligibility checks; both arguments are current-class IDs, not identities. */
#include "affinity_config.h"
extern unsigned char lc_affinity_members[256];
extern signed char lc_affinity_strengths[];

int lc_leader_affinity(unsigned int leader, unsigned int member)
{
    unsigned int group;
    if (leader == 0 || leader >= 256 || member >= 256)
        return 0;
    group = lc_affinity_members[member];
    if (group >= LC_AFFINITY_GROUPS)
        return 0;
    return lc_affinity_strengths[leader * LC_AFFINITY_GROUPS + group];
}
