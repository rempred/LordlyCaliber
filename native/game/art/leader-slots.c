/* Complete leader normalizer, z64 [2AB574,2AB6BC), live 8023BE14.
 * Re-read each mutable scene slot, preserve both sides of a swap, and update
 * Actor slot fields. Registered linked primaries remain leader candidates;
 * their companions and unregistered high poses retain the original skip rule.
 * The primary query lives in body selection in this same resident Director slab.
 */
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
#define B(p, n) (*(u8 *)((u8 *)(p) + (n)))
#define H(p, n) (*(s16 *)((u8 *)(p) + (n)))
#define W(p, n) (*(u32 *)((u8 *)(p) + (n)))
extern u8 *lc_scene, *lc_roster;
extern s32 lc_private_pose(s32, s32, s32, s32);
void lc_leader_slots(void) {
    int i, target;
    u8 *actor, *row, *other;
    u32 owner, flags;
    for (i = 0; i < 28; i++) {
        actor = (u8 *)W(lc_scene, 24 + i * 4);
        if (!actor)
            continue;
        row = lc_roster + 452 + B(actor, 327) * 248;
        owner = W(row, 76);
        if ((owner == 10 || owner == 25 || owner == 123) && H(actor, 312) >= 50 &&
            (!LC_PRIVATE_POSE_COUNT ||
             lc_private_pose(W(actor, 232), W(actor, 236), B(actor, 326) * 2 + H(actor, 314),
                             H(actor, 312)) < 0))
            continue;
        flags = W(row, 64);
        if (!(flags & 512))
            continue;
        target = (flags >> 8) & 1;
        if (target == i)
            continue;
        other = (u8 *)W(lc_scene, 24 + target * 4);
        W(lc_scene, 24 + target * 4) = (u32)actor;
        W(lc_scene, 24 + i * 4) = (u32)other;
        if (other)
            W(other, 228) = i;
        W(actor, 228) = target;
    }
}
