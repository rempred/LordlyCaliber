extern unsigned char lc_movement_kinds[];
extern float lc_movement_speeds[];
#include "movement_config.h"
#include "save-cache.h"
extern unsigned char lc_movement_sources[],lc_movement_groups[],lc_movement_characters[];
extern unsigned char lc_movement_records[],lc_movement_priority[];
void lc_movement_refresh_saved(void) {
    lc_refresh_movement_cache(lc_movement_sources,lc_movement_groups,lc_movement_characters,
        lc_movement_records,LC_MOVE_CLASSES,lc_movement_priority,LC_MOVE_COUNT);
}
int lc_movement_kind(unsigned int id) {
    return id < 256 ? lc_movement_kinds[id] : 7;
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
