#include "game/shop_price_interface.h"

typedef struct ShopDate {
    u16 year;
    u8 month;
    u8 day;
} ShopDate;

extern u16 D_80196A30;
extern u8 D_80196A32;
extern u8 D_80196A33;
/* The accepted shop-overlay call target maps to func_000424bc at this live address. */
extern void func_8016C5BC(ShopDate *date);

u32 func_0019BD14(void)
{
    s32 year = D_80196A30;
    s32 month = D_80196A32;
    s32 day = D_80196A33;
    s32 elapsed = 0;
    ShopDate date;
    s32 increment;
    s32 price;

    date.year = 0xFB;
    date.month = 4;
    date.day = 1;
    do {
        func_8016C5BC(&date);
        if (year == date.year && month == date.month && day == date.day) {
            break;
        }
        elapsed++;
    } while (elapsed < 9999);

    increment = elapsed * 20;
    price = 60000;
    if (increment < 30001) {
        price = increment + 30000;
    }
    return price;
}
