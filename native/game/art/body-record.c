/* Adapted accepted func_00204F34 in accepted/combat_pose_metadata.c.
 * Class pose reader z64 204F34..2050AC, live 801C1AA4.
 * The incoming stack arguments remain in their ABI slots across the resource
 * calls, leaving room for the registration query in this same code allocation.
 * Output operands are written in the original order, including nullable outputs.
 * The original reader requires a valid registry allocation before reading its count.
 */
#include "pose_operands.h"
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
#define W(p, n) (*(u32 *)((u8 *)(p) + (n)))
#define H(p, n) (*(s16 *)((u8 *)(p) + (n)))
extern int lc_body_handle(s32, s32, s32, s32);
extern u8 *lc_pose_program(int);
extern u8 lc_widths[];
extern u8 *lc_class_handles;
int lc_body_record(s32 source, s32 owner, s32 flag_a, s32 flag_b, volatile s32 state,
                   volatile s32 cursor, u8 *volatile a, u8 *volatile b, u8 *volatile c) {
    u8 *bank = lc_pose_program(lc_body_handle(source, owner, flag_a, flag_b));
    u8 *p;
    u32 count, i;
    int width;
    p = bank;
    if (p)
        p += W(p, state * 4);
    count = *p++;
    for (i = 0; i < count; i++) {
        width = lc_widths[*p];
        if (i == cursor) {
            return lc_pose_operands(p, width, a, b, c);
        }
        p += width;
    }
    return 0;
}

typedef struct {
    u32 key;
    s16 secondary;
    u16 unused;
} PoseMap;
/* Actor states arrive as their stored halfword bits. The signed companion
 * sentinel -1 cannot match an unsigned state, including the 0xffff bit pattern. */
s32 lc_registered_pose(s32 source, s32 owner, s32 flags, u16 state) {
#if LC_PRIVATE_POSE_COUNT
    PoseMap *end = (PoseMap *)lc_class_handles, *row = end - LC_PRIVATE_POSE_COUNT;
    u32 key, entry;
    if ((u32)(source | owner | flags) > 255)
        return 0;
    key = ((u32)source << 16) | ((u32)owner << 8) | (u32)flags;
    do {
        entry = row->key;
        if ((entry >> 8) == key && (row->secondary == state || (entry & 255) == (u32)state))
            return 1;
        row++;
    } while (row != end);
    return 0;
#else
    return 0;
#endif
}
