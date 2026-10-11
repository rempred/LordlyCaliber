/* Host reconstruction of US Rev0 Army ROM BEA34..BEA90, RAM801D1FD4.
 * The complete 92-byte helper matches the captured Army overlay. Not accepted
 * matching C. The caller separately validates roster and squad ownership. */
#include "leader.h"
int lordly_army_fixed_leader(uint8_t member, uint8_t primary_class) {
    if (member == 0 || member >= 100)
        return 0;
    return (primary_class >= 81 && primary_class <= 83) || primary_class == 163;
}
