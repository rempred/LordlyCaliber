#ifndef LORDLY_POSE_OPERANDS_H
#define LORDLY_POSE_OPERANDS_H
/* Adapted from the identical operand switches in accepted func_00204F34 and
 * func_002050AC (game/accepted/combat_pose_metadata.c). Callers resolve the
 * directory and validate host storage; ROM callers keep the native contract.
 * Shared by the PC decoder and the ROM reader compiled during user export. */
#ifdef _MSC_VER
#define LC_POSE_INLINE __inline
#else
#define LC_POSE_INLINE __inline__
#endif
static LC_POSE_INLINE unsigned int lc_pose_operands(const unsigned char *record, int width,
    unsigned char *a, unsigned char *b, unsigned char *c) {
    /* Equivalent to the accepted cases 2/3/4, with compact code so the private
     * registration query can still share the original ROM reader allocation. */
    if (width >= 2 && width <= 4) {
        if (a) *a = record[1];
        if (width >= 3 && b) *b = record[2];
        if (width == 4 && c) *c = record[3];
    }
    return *record;
}
#undef LC_POSE_INLINE
#endif
