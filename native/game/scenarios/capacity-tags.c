#include "enemy-capacity.h"
#include "capacity-save.h"

void lc_capacity_tags_save(unsigned char *data)
{
    unsigned int i;
    for (i = 0; i < LC_ENEMY_HANDLES; ++i) *data++ = lc_enemy_types[i];
    /* The unchanged normal descriptor saves handles100..219. The extension
     * carries every other slot, including unused/sentinel payload verbatim. */
    for (i = 0; i < LC_ENEMY_HANDLES; ++i) if (i < 100 || i >= 220) {
        *data++ = (unsigned char)(lc_enemy_soldier_hp[i] >> 8);
        *data++ = (unsigned char)lc_enemy_soldier_hp[i];
        *data++ = lc_enemy_soldier_state[i];
    }
}
void lc_capacity_tags_load(const unsigned char *data)
{
    unsigned int i;
    for (i = 0; i < LC_ENEMY_HANDLES; ++i) lc_enemy_types[i] = *data++;
    for (i = 0; i < LC_ENEMY_HANDLES; ++i) if (i < 100 || i >= 220) {
        lc_enemy_soldier_hp[i] = (unsigned short)(((unsigned int)data[0] << 8) | data[1]);
        lc_enemy_soldier_state[i] = data[2];
        data += 3;
    }
}
