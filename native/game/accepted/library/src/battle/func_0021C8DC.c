typedef signed int s32;
typedef unsigned char u8;
typedef float f32;

struct BattleQueueEntry {
    u8 pad_00[8];
    s32 field_08;
    u8 pad_0C[4];
    f32 field_10;
    u8 pad_14[0xAC];
};

s32 func_0021C8DC(struct BattleQueueEntry **left_ptr, struct BattleQueueEntry **right_ptr)
{
    struct BattleQueueEntry *left;
    struct BattleQueueEntry *right;

    left = *left_ptr;
    right = *right_ptr;
    if (left->field_10 < right->field_10) {
        return -1;
    }
    if (right->field_10 < left->field_10) {
        return 1;
    }
    if (left->field_08 < right->field_08) {
        return 1;
    }
    if (right->field_08 < left->field_08) {
        return -1;
    }
    return left - right;
}
