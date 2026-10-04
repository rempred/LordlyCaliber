#include "actor_presentation.h"
/* Portable adaptation of accepted func_00201670 / func_002016F4,
 * Decomp 497181d8a6ba4bc32c35a465fccd261abe2e0c69. The host resolves the
 * current class record and the two interleaved resource-table lanes. */
float lordly_body_speed(uint8_t source_owner, int32_t owner_context,
                       uint8_t source_entry, uint8_t owner_entry) {
    if (source_owner == owner_context && source_entry != 255)
        return (float)source_entry / 16.0f;
    return (float)owner_entry / 16.0f;
}
int lordly_actor_snap(LordlyActorPosition *p, float x, float z, double tx, double tz) {
    int result = 0;
    if ((double)p->x < (double)x + tx && (double)x - tx < (double)p->x) {
        p->x = x;
        result = 1;
        tx = tz;
    }
    if ((double)p->z < (double)z + tx && (double)z - tx < (double)p->z) {
        p->z = z;
        result += 2;
    }
    return result;
}
