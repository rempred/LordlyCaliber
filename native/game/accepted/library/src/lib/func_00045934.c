/* Retail repeats this test in each slot and makes no classification calls. */
static __inline__ int hasWeaponType(int itemId)
{
    int type = ((unsigned char *)0x8018C410)[(itemId & 0xFFFF) * 32];

    if (type != 0) {
        if (type < 14) {
            return 1;
        }
        if (type == 24) {
            return 1;
        }
    }
    return 0;
}

int func_00045934(int item0, int item1, int item2, int item3)
{
    if (hasWeaponType(item0)) {
        return item0 & 0xFFFF;
    }
    if (hasWeaponType(item1)) {
        return item1 & 0xFFFF;
    }
    if (hasWeaponType(item2)) {
        return item2 & 0xFFFF;
    }
    if (hasWeaponType(item3)) {
        return item3 & 0xFFFF;
    }
    return 0;
}
