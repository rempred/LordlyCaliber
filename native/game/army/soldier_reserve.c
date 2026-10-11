/* Host reconstruction of US Rev0 Army ROM B86F8..B876C (group count),
 * B86C4..B86F8 (free ID), and B81CC..B8258 (new group HP).
 * Complete helper/block bytes match the captured Army image at delta801135A0.
 * Not accepted matching C. Preserve native truncation, including count1 at HP0.
 * The editor validates pool, allocation and packed-save bounds separately. */
#include "soldier_reserve.h"
uint16_t lordly_soldier_count(uint16_t hp, uint16_t maximum_hp) {
    int remaining = hp;
    const int portion = maximum_hp / 3;
    uint16_t count = 0;
    do {
        remaining -= portion;
        ++count;
    } while (remaining > 0 && count < 3);
    return count;
}
uint16_t lordly_soldier_created_hp(uint16_t maximum_hp, uint8_t count) {
    return count == 3 ? maximum_hp : (uint16_t)((maximum_hp / 3) * count);
}
int lordly_soldier_free_id(const uint16_t hp[120]) {
    int index = 0;
    while (index < 120 && hp[index])
        ++index;
    return index + 100;
}
