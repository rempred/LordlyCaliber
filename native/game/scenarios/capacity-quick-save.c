#include "enemy-capacity.h"
#include "capacity-save.h"

/* Existing packed codec and name-conversion services, bound by the linker.
 * The descriptor retains every retail field and grows only the enemy/source
 * counts. Permanent scratch avoids hundreds of tiny allocations during saving.
 */
extern unsigned char lc_capacity_name_work[LC_ENEMY_HANDLES][16];
extern unsigned char lc_capacity_quick_descriptor[];
void lc_capacity_pack(unsigned char *data, const void *descriptor);
void lc_capacity_unpack(const unsigned char *data, const void *descriptor);
void lc_capacity_name_encode(const unsigned char *source, unsigned char *target);
void lc_capacity_name_decode(const unsigned char *source, unsigned char *target);

void lc_capacity_quick_save(unsigned char *data)
{
    unsigned int i, j;
    for (i = 0; i < LC_ENEMY_HANDLES; ++i) {
        for (j = 0; j < 16; ++j) lc_capacity_name_work[i][j] = lc_enemy_characters[i][j];
        lc_capacity_name_encode(lc_capacity_name_work[i], lc_enemy_characters[i]);
    }
    lc_capacity_pack(data, lc_capacity_quick_descriptor);
    for (i = 0; i < LC_ENEMY_HANDLES; ++i)
        for (j = 0; j < 16; ++j) lc_enemy_characters[i][j] = lc_capacity_name_work[i][j];
    lc_capacity_tags_save(data + LC_CAPACITY_QUICK_BYTES);
}
void lc_capacity_quick_load(const unsigned char *data)
{
    unsigned int i, j;
    unsigned char name[16];
    for (i = 0; i < LC_ENEMY_HANDLES; ++i)
        for (j = 0; j < 52; ++j) lc_enemy_characters[i][j] = 0;
    lc_capacity_unpack(data, lc_capacity_quick_descriptor);
    for (i = 0; i < LC_ENEMY_HANDLES; ++i) {
        for (j = 0; j < 16; ++j) name[j] = lc_enemy_characters[i][j];
        lc_capacity_name_decode(name, lc_enemy_characters[i]);
    }
    lc_capacity_tags_load(data + LC_CAPACITY_QUICK_BYTES);
}
