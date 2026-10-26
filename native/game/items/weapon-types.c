/* Expanded equipment classification. The shop ordering follows accepted
 * func_0019B800; only its repeated type switch becomes the shared group lookup.
 * Each operation is compiled into its authenticated original function extent. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
extern u8 lc_item_types[][32];
extern u8 lc_type_groups[256];
extern int lc_is_weapon(u16 item);
extern u8 lc_equipment_group(u32 item);
extern u32 D_8021A010[50];
extern u8 D_8021A114[6];
extern u16 D_8018C414[][16];
extern int lc_special_price(void);

#if LC_TYPE_OPERATION == 1
int lc_is_weapon(u16 item) {
    u32 type = lc_item_types[item][0];
    return (type - 1 < 13) | (type == 24) | (type - 26 < 229);
}
#elif LC_TYPE_OPERATION == 2
u16 lc_first_weapon(u16 first, u16 second, u16 third, u16 fourth) {
    if (lc_is_weapon(first)) return first;
    if (lc_is_weapon(second)) return second;
    if (lc_is_weapon(third)) return third;
    if (lc_is_weapon(fourth)) return fourth;
    return 0;
}
#elif LC_TYPE_OPERATION == 3
u8 lc_equipment_group(u32 item) {
    return lc_type_groups[lc_item_types[item][0]];
}
#elif LC_TYPE_OPERATION == 4
void lc_sort_shop(s32 count) {
    s32 first, second, start, category_first;
    u8 *category_count;
    u32 *price_entries, special_id;
    for (category_first = 0; category_first < count - 1; category_first++) {
        for (first = category_first + 1; first < count; first++) {
            u32 first_category = lc_equipment_group(D_8021A010[category_first]);
            u32 second_category = lc_equipment_group(D_8021A010[first]);
            if (second_category < first_category) {
                u32 temporary = D_8021A010[category_first];
                D_8021A010[category_first] = D_8021A010[first];
                D_8021A010[first] = temporary;
            }
        }
    }
    start = 0;
    price_entries = D_8021A010;
    special_id = 250;
    category_count = D_8021A114;
    do {
        if (*category_count != 0) {
            for (first = 0; first < *category_count - 1; first++) {
                for (second = first + 1; second < *category_count; second++) {
                    s32 first_price, second_price;
                    u32 item_id = price_entries[start + first];
                    if (item_id == special_id) first_price = lc_special_price();
                    else {
                        item_id <<= 5;
                        first_price = *(u16 *)((u8 *)D_8018C414 + item_id);
                    }
                    item_id = price_entries[second + start];
                    if (item_id == special_id) second_price = lc_special_price();
                    else second_price = D_8018C414[item_id][0];
                    if (second_price < first_price) {
                        u32 temporary = price_entries[start + first];
                        price_entries[start + first] = price_entries[second + start];
                        price_entries[second + start] = temporary;
                    }
                }
            }
        }
        start += *category_count;
        category_count++;
    } while ((s32)category_count < (s32)(D_8021A114 + 6));
}
#endif
