#include "common/types.h"

/* Retail reads the first byte at a 32-byte equipment-record stride. */
extern u8 D_8018C410[][32];

u8 func_0019B1B0(u32 equipment_id)
{
    u8 category = 0;

    /* These are category indices used by the Shop counters, not type IDs. */
    switch (D_8018C410[equipment_id][0]) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 24:
        category = 1;
        break;
    case 14:
    case 15:
        category = 3;
        break;
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
        category = 4;
        break;
    case 21:
    case 22:
        category = 0;
        break;
    case 23:
        category = 2;
        break;
    case 25:
        category = 5;
        break;
    default:
        break;
    }

    return category;
}

