/* Director body selection, z64 2A9364..2A985C, live 80239C04.
 * Reconstructed and compared against the original complete routine. Export
 * compiles this source into that same allocation. The generated mapping rows
 * immediately precede the relocated class-handle table; no additional hook.
 * Keep recursive arguments unresolved: each child resolves its own sentinels.
 */
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
#define B(p, n) (*(u8 *)((u8 *)(p) + (n)))
#define H(p, n) (*(s16 *)((u8 *)(p) + (n)))
#define U(p, n) (*(u16 *)((u8 *)(p) + (n)))
#define W(p, n) (*(u32 *)((u8 *)(p) + (n)))
extern u8 *lc_scene, *lc_roster;
extern s32 lc_presentation;
extern int lc_appearance0(u8, u8), lc_appearance1(u8, u8), lc_appearance2(u8, u8),
    lc_appearance3(u8, u8);
extern int lc_equipment(u16, u16, u16, u16);
extern void lc_fill(void *, int, int), lc_zero(void *, int);
extern void lc_prepare(s32 *, s32 *, s32 *, s32 *, s32 *, int), lc_pose(u8 *);
static int classify(u8 *);
extern u8 *lc_class_handles;
s32 lc_private_pose(s32, s32, s32, s32);
int lc_body_select(s32 slot, s32 source, s32 owner, s32 state, s32 flag_b, s32 flag_a,
                   u8 keep_ground, u8 reuse_identity) {
    u8 *actor, *row, *child;
    s32 src = source, own = owner, pose = state, b = flag_b, a = flag_a, item = 0;
    int i, j;
    if (slot == -1)
        return slot;
    actor = (u8 *)W(lc_scene, 24 + slot * 4);
    if (!actor)
        return slot;
    if (src == -1)
        src = W(actor, 0xe8);
    if (own == -1)
        own = W(actor, 0xec);
    if (pose == -1)
        pose = H(actor, 0x138);
    if (b == -1)
        b = H(actor, 0x13a);
    if (classify(actor) == 2) {
        if (pose >= 50) {
            pose = lc_private_pose(src, own, (a == -1 ? B(actor, 0x146) : a) * 2 + b, pose);
            if (pose < 0)
                return slot;
        } else
            pose += 50;
    }
    W(actor, 0xe4) = (u8)slot;
    W(actor, 0xe8) = src;
    H(actor, 0x138) = pose;
    W(actor, 0xec) = own;
    B(actor, 0x13d) = 1;
    H(actor, 0x134) = pose;
    W(actor, 0xf0) = -1;
    W(actor, 0xf4) = 0;
    W(actor, 0xf8) = 0;
    W(actor, 0x12c) = 0;
    W(actor, 0x128) = 0;
    W(actor, 0x130) = 0x3f800000;
    W(actor, 0xe0) = 0x8022f2cc;
    H(actor, 0x13a) = b;
    B(actor, 0x13c) = lc_presentation;
    if (!keep_ground)
        B(actor, 0x145) = 1;
    lc_fill(actor, 255, 16);
    lc_zero(actor + 16, 16);
    if (a != -1)
        B(actor, 0x146) = a;
    a = B(actor, 0x146);
    if (!reuse_identity && B(actor, 0x147) == 255) {
        for (i = 0; i < 28; i++) {
            for (j = 0; j < 28; j++) {
                child = (u8 *)W(lc_scene, 24 + j * 4);
                if (child && B(child, 0x147) == i)
                    break;
            }
            if (j == 28)
                break;
        }
        B(actor, 0x147) = i == 28 ? 255 : i;
        B(actor, 0x148) = 1;
        row = lc_roster + 0x1c4 + B(actor, 0x147) * 248;
        U(row, 0x36) = lc_appearance0(src, own);
        U(row, 0x38) = lc_appearance1(src, own);
        U(row, 0x3a) = lc_appearance2(src, own);
        U(row, 0x3c) = lc_appearance3(src, own);
    } else
        row = lc_roster + 0x1c4 + B(actor, 0x147) * 248;
    item = (u16)lc_equipment(U(row, 0x36), U(row, 0x38), U(row, 0x3a), U(row, 0x3c));
    lc_prepare(&src, &own, &b, &a, &item, 1);
    lc_pose(actor);
    if (classify(actor) == 1)
        for (i = 0; i < 28; i++) {
            child = (u8 *)W(lc_scene, 24 + i * 4);
            if (child && B(child, 0x147) == B(actor, 0x147) && classify(child) == 2)
                lc_body_select(i, source, owner, state, flag_b, flag_a, keep_ground,
                               reuse_identity);
        }
    return slot;
}
static int classify(u8 *actor) {
    u8 *row;
    if (B(actor, 0x147) == 255)
        return 0;
    row = lc_roster + 0x1c4 + B(actor, 0x147) * 248;
    if (!W(row, 0x48) || (W(row, 0x4c) != 10 && W(row, 0x4c) != 25 && W(row, 0x4c) != 123))
        return 0;
    return 1 + (B(actor, 0x149) == 1);
}

s32 lc_private_pose(s32 source, s32 owner, s32 flags, s32 state) {
#if LC_PRIVATE_POSE_COUNT
    u8 *end = lc_class_handles, *row = end - LC_PRIVATE_POSE_COUNT * 8;
    u32 key;
    if ((u32)(source | owner | state | flags) > 255)
        return -2;
    key = ((u32)source << 24) | ((u32)owner << 16) | ((u32)flags << 8) | (u32)state;
    do {
        if (W(row, 0) == key)
            return H(row, 4);
        row += 8;
    } while (row != end);
    return -2;
#else
    return -2;
#endif
}
