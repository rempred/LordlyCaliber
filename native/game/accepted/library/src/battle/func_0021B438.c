typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned char u8;
typedef float f32;

extern void *func_0020C478(s32 index);
extern s32 func_0020C2C0(void *record);

void func_0021B438(void)
{
    s32 owner_index;
    u32 owner_slot;
    s32 other_index;
    s32 strongest_adjustment;
    u32 other_slot;
    u8 *owner;
    u8 *owner_check;
    u8 *owner_cursor;
    u8 *other;
    u8 *other_cursor;
    u8 *actor;
    u8 *other_actor;
    s32 group_value;
    s32 owner_value;
    s32 other_value;
    s32 distance;
    s32 dx;
    s32 dy;
    s32 abs_strongest;
    s32 abs_adjustment;
    s16 current;

    for (owner_index = 0; owner_index < 20; owner_index++) {
        owner = (u8 *)func_0020C478(owner_index);
        if (func_0020C2C0(owner) != 0) {
            continue;
        }
        owner_check = owner;
        if ((*(s32 *)(owner_check + 0x40) & 0x800) != 0) {
            continue;
        }

        /* Keep a body for each class. The pinned compiler merges equal case
         * bodies after allocation; grouping them earlier changes the owner
         * lifetime and permits the inner table address to move. */
        switch (*(s32 *)(owner_check + 0x74)) {
        case 2:
            group_value = 10;
            break;
        case 8:
            group_value = 10;
            break;
        case 60:
            group_value = 0;
            break;
        case 1:
            group_value = 1;
            break;
        case 6:
            group_value = 20;
            break;
        case 7:
            group_value = 20;
            break;
        case 12:
            group_value = 20;
            break;
        case 14:
            group_value = 20;
            break;
        case 19:
            group_value = 20;
            break;
        case 33:
            group_value = 20;
            break;
        case 45:
            group_value = 20;
            break;
        case 48:
            group_value = 20;
            break;
        case 50:
            group_value = 20;
            break;
        default:
            group_value = 5;
            break;
        }
        owner_value = group_value;
        owner_slot = 0;
        owner_cursor = owner;
        do {
            actor = *(u8 **)owner_cursor;
            if (actor == 0) {
                continue;
            }

            strongest_adjustment = 0;
            for (other_index = 0; other_index < 20; other_index++) {
                other = (u8 *)func_0020C478(other_index);
                if (func_0020C2C0(other) != 0) {
                    continue;
                }
                if (owner == other || (*(s32 *)(other + 0x40) & 0x800) != 0) {
                    continue;
                }

                switch (*(s32 *)(other + 0x74)) {
                case 2:
                    group_value = 10;
                    break;
                case 8:
                    group_value = 10;
                    break;
                case 60:
                    group_value = 0;
                    break;
                case 1:
                    group_value = 1;
                    break;
                case 6:
                    group_value = 20;
                    break;
                case 7:
                    group_value = 20;
                    break;
                case 12:
                    group_value = 20;
                    break;
                case 14:
                    group_value = 20;
                    break;
                case 19:
                    group_value = 20;
                    break;
                case 33:
                    group_value = 20;
                    break;
                case 45:
                    group_value = 20;
                    break;
                case 48:
                    group_value = 20;
                    break;
                case 50:
                    group_value = 20;
                    break;
                default:
                    group_value = 5;
                    break;
                }
                other_value = group_value;
                other_slot = 0;
                other_cursor = other;
                do {
                    other_actor = *(u8 **)other_cursor;
                    if (other_actor == 0) {
                        continue;
                    }

                    dx = *(s16 *)(actor + 0x1C) - *(s16 *)(other_actor + 0x1C);
                    if (dx <= 0) {
                        dx = *(s16 *)(other_actor + 0x1C) - *(s16 *)(actor + 0x1C);
                    }
                    dy = *(s16 *)(actor + 0x20) - *(s16 *)(other_actor + 0x20);
                    if (dy <= 0) {
                        dy = *(s16 *)(other_actor + 0x20) - *(s16 *)(actor + 0x20);
                    }
                    if (dx < 20 && dy < 20) {
                        distance = (s32)__builtin_sqrtf((f32)(dx * dx + dy * dy));
                        /* Each signed half is formed in its own branch; the
                         * pinned compiler then shares the division tail. */
                        if (owner_value == other_value) {
                            if (*(s16 *)(actor + 0x20) >= *(s16 *)(other_actor + 0x20)) {
                                distance = (20 - distance) / 2;
                            } else {
                                distance = (distance - 20) / 2;
                            }
                        } else if (owner_value < other_value) {
                            if (*(s16 *)(actor + 0x20) < *(s16 *)(other_actor + 0x20)) {
                                distance = distance - 20;
                            } else {
                                distance = 20 - distance;
                            }
                        } else {
                            distance = 0;
                        }

                        abs_strongest = strongest_adjustment >= 0 ? strongest_adjustment : -strongest_adjustment;
                        abs_adjustment = distance >= 0 ? distance : -distance;
                        if (abs_strongest < abs_adjustment) {
                            strongest_adjustment = distance;
                        }
                    }
                } while (other_cursor += 4, other_slot++, other_slot < 3);

            }
            current = *(s16 *)(actor + 0x26);
            if (current < strongest_adjustment) {
                if (strongest_adjustment - current < 3) {
                    *(s16 *)(actor + 0x26) = strongest_adjustment;
                } else {
                    *(s16 *)(actor + 0x26) = current + 3;
                }
            } else if (current - strongest_adjustment < 2) {
                *(s16 *)(actor + 0x26) = strongest_adjustment;
            } else {
                *(s16 *)(actor + 0x26) = current - 2;
            }
        } while (owner_cursor += 4,
                 owner_slot++, owner_slot < 3);
    }
}
