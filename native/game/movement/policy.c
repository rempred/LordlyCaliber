extern unsigned char lc_movement_kinds[];
extern float lc_movement_speeds[];
extern unsigned char lc_movement_rules[];
#include "movement_config.h"
#include "save-cache.h"
extern unsigned char lc_movement_sources[],lc_movement_groups[],lc_movement_characters[];
extern unsigned char lc_movement_records[],lc_movement_priority[];
void lc_movement_refresh_saved(void) {
    lc_refresh_movement_cache(lc_movement_sources,lc_movement_groups,lc_movement_characters,
        lc_movement_records,LC_MOVE_CLASSES,lc_movement_priority,LC_MOVE_COUNT);
}
int lc_movement_kind(unsigned int id) {
    if (id >= 256) return 7;
    if (lc_movement_rules[id * 16 + 3]) return 1;
    if (!lc_movement_rules[id * 16]) return 7;
    return lc_movement_kinds[id] >= 2 && lc_movement_kinds[id] <= 6 ? lc_movement_kinds[id] : 2;
}
int lc_movement_rule(unsigned int id, unsigned int field) {
    return id < 256 && field < 16 ? lc_movement_rules[id * 16 + field] : 0;
}
int lc_movement_profile_selector(unsigned int id, int current) {
    int selected = lc_movement_rule(id, 5);
    if (selected == 2) return 0x4a;
    if (selected == 3) return 0x49;
    if (selected == 4) return 0x47;
    if (selected == 0 && (current == 0x4a || current == 0x49 ||
        (unsigned int)(current - 0x24) < 2 || (unsigned int)(current - 0x27) < 2 ||
        current == 0x29 || current == 0x45 || current == 0x46 || current == 0x79 ||
        current == 0x47 || current == 0x48)) return current;
    return 0x24;
}
float lc_movement_factor(unsigned int id, float penalty, int group) {
    float base = id < 256 ? lc_movement_speeds[id] : 35.0f;
    float value = base - penalty;
    if (group) {
        float minimum = base < 5.0f ? base : 5.0f;
        if (value < minimum) value = minimum;
    } else if (base != 35.0f && value < 1.0f) {
        value = 1.0f;
    }
    return value;
}
