#include "game/shop_price_interface.h"

typedef struct { u32 w0, w1; } ShopGfx;
typedef struct { s16 field00, field02, field04, field06, field08; } ShopRow;
/* View of the observed word at34; no total object extent is inferred. */
typedef struct { u8 pad00[0x34]; s32 field34; } ShopWindowWidthView;
extern u32 *D_800E9BA0;
extern ShopRow *D_80219F40;
extern u8 D_8021A121, D_8021A122, D_8021A123, D_80219D24;
extern u32 D_8021A0D8[], D_8021A010[];
extern u16 D_8018E6D2[][6], D_8018C414[][16];
extern u8 D_80193AC2[][4];
extern u8 D_80196B02[];
extern u32 D_80196A6C;
extern s32 D_80214F4C;
extern u8 D_80211558[], D_80213648[], D_80210258[], D_802102B8[];
extern u8 D_80212318[], D_80211DD0[];
extern u8 *D_801F36C0, *D_801F0B90, *D_8021A12C;
extern void *D_8021A128;
extern u8 func_0019B21C(u8 category);
extern u8 func_0019B26C(u16 selected, u8 mode);
extern u16 func_8016B738(u16 item);
extern u16 func_8016B6FC(u16 item);
extern u8 func_8016F520(u16 item);
extern void func_8017C4B4(s32 value);
extern void func_80200F70(void *a, void *b, s32 width, s32 height, s32 mode);
extern void func_80200DD0(s32 x, s32 y, s32 size);
extern void func_802027DC(void);
extern void func_802004C4(s32 x, s32 y, u32 value, s32 digits);
extern void func_802012D8(void *data);
extern void func_80201384(s32 x, s32 y, s32 width, s32 height);
extern void func_801818A0(s32 x, s32 y);

#define W32(off) (*(s32 *)(window + (off)))
#define ROW() (&D_80219F40[D_8021A121])
#define COMMAND(a,b) { \
    ShopGfx *g = (ShopGfx *)D_800E9BA0; \
    D_800E9BA0 = (u32 *)(g + 1); \
    g->w0 = (u32)(a); g->w1 = (u32)(b); \
}
#define CLIP4(v) (((u32)(v) << 2) & \
                  (u32)(~(s32)(s16)((u32)(v) << 2) >> 31) & 0xFFC)

void func_0019A294(u8 *window, u32 slot_word)
{
    s32 x, y, quantity;
    u32 item, price, index;
    slot_word = (slot_word << 1) + (u32)window;
    if (*(s16 *)(slot_word + 0x18) == 0) {
        return;
    }
    x = W32(0x2C);
    y = W32(0x30);
    {
        u32 sync = 0xE7000000;
        ShopGfx *g = (ShopGfx *)D_800E9BA0;
        s32 value;
        g->w0 = sync;
        g->w1 = 0;
        value = *(s16 *)(slot_word + 0x18);
        D_800E9BA0 = (u32 *)(g + 1);
        func_8017C4B4(value);
    }
    if (D_8021A121 == D_8021A122) {
        item = D_8021A0D8[ROW()->field00];
        price = D_8018E6D2[item][0];
        quantity = func_0019B26C((u16)item, 0);
    } else {
        u32 category = func_0019B21C(D_8021A121);
        item = D_8021A010[ROW()->field00 + category];
        if (item == 250) {
            price = func_0019BD14();
        } else {
            price = D_8018C414[item][0];
        }
        quantity = func_0019B26C((u16)item, 1);
    }
    {
    s32 left, line_y;
    u32 *money;
    COMMAND(0xDE000000, D_80213648);
    func_80200F70(D_80211558, D_8021A128, 80, 17, 2);
    left = x + 28;
    line_y = y + 9;
    func_80200DD0(left, line_y, 25);
    {
        s32 caption_x = x + 13;
        if ((D_8021A123 & 0x80) == 0) {
            caption_x = x + 20;
        }
        func_80200DD0(caption_x, y + 19, 52);
    }
    func_80200DD0(left, y + 39, 25);
    func_802027DC();
    func_802004C4(x + 12, y + 9, quantity ? (D_8021A123 & 0x7F) : 0, 2);
    func_802004C4(x + 38, y + 9, quantity, 2);
    money = &D_80196A6C;
    func_802004C4(x + 74, y + 39, *money, 7);
    if (*money < price) {
        func_802012D8(D_802102B8);
    } else {
        func_802012D8(D_80210258);
    }
    func_802004C4(x + 86, y + 9, price * (D_8021A123 & 0x7F), 5);
    func_802012D8(D_80210258);
    }
    if (D_8021A121 == D_8021A122) {
        item &= 0xFFFF;
        index = func_8016B738(item);
        if (index == 511) {
            s32 line_y = y + 39;
            func_802004C4(x + 12, line_y, 0, 2);
            func_802004C4(x + 38, line_y, 0, 2);
        } else {
            if (!func_8016F520(item)) {
                func_802004C4(x + 12, y + 39, D_80193AC2[index][0], 2);
            }
            func_802004C4(x + 38, y + 39, D_80193AC2[index][1], 2);
        }
    } else {
        s32 resource_x;
        item = func_8016B6FC((u16)item);
        resource_x = x + 12;
        if (item == 511) {
            s32 line_y = y + 39;
            func_802004C4(resource_x, line_y, 0, 2);
            func_802004C4(x + 38, line_y, 0, 2);
        } else {
            s32 line_y;
            item <<= 2;
            line_y = y + 39;
            func_802004C4(resource_x, line_y, D_80196B02[item], 2);
            func_802004C4(x + 38, line_y, D_80196B02[item + 1], 2);
        }
    }
    {
    s32 left, line_y;
    s32 mode = 2;
    func_80201384(W32(0x2C) - 8, W32(0x30) - 8, ((ShopWindowWidthView *)window)->field34 + 4, W32(0x38) + 4);
    func_80200F70(D_801F36C0 + 0x320, D_801F0B90 + 0x2828, 48, 32, mode);
    line_y = y - 6;
    func_80200DD0(x + 80, line_y, 35);
    func_80200DD0(x + 126, y + 10, 26);
    left = x + 10;
    func_80200DD0(left, line_y, 77);
    func_80200F70(D_8021A12C + 0x220, D_8021A12C + 0xB868, 32, 7, mode);
    func_80200DD0(left, line_y, 120);
    }
    {
    func_80200F70(D_801F36C0 + 0x300, D_801F0B90 + 0x27E8, 16, 8, 4);
    {
        ShopGfx *g = (ShopGfx *)D_800E9BA0;
        ShopGfx *texture_command;
        u32 texture_word;
        s32 wrapped_x;
        s32 width;
        u32 upper_y, scaled_width;
        D_800E9BA0 = (u32 *)(g + 1);
        g[0].w0 = 0xE7000000;
        g[0].w1 = 0;
        /* This observed member read and the following real wrapped-x producer
         * retain the original scheduling with the matching compiler. */
        width = ((ShopWindowWidthView *)window)->field34;
        wrapped_x = (s16)((u32)x << 2);
        D_800E9BA0 = (u32 *)(g + 2);
        g[1].w1 = (CLIP4(x) << 12) | CLIP4(y + 26);
        scaled_width = (u32)width << 2;
        upper_y = CLIP4(y + 31) | 0xE4000000;
        scaled_width &= (u32)(~(s32)(s16)((u32)width << 2) >> 31);
        scaled_width &= 0xFFC;
        g[1].w0 = (scaled_width << 12) | upper_y;
        texture_command = g + 2;
        D_800E9BA0 = (u32 *)(g + 3);
        texture_command->w0 = 0xE1000000;
        if (wrapped_x < 0) {
            s32 scaled = wrapped_x * 8;
            texture_word = (480 - (scaled & -(scaled < 1))) << 16;
        } else {
            texture_word = 0x01E00000;
        }
        texture_command->w1 = texture_word;
        if ((s32)((u32)(y + 26) << 2) < 0) {
            s32 scaled = (s32)((u32)(y + 26) << 18) >> 13;
            s32 nonpositive = scaled < 1;
            nonpositive = -nonpositive;
            scaled &= nonpositive;
            scaled = -scaled;
            texture_command->w1 = texture_word | (u16)scaled;
        }
    }
    COMMAND(0xF1000000, 0x04000400);
    COMMAND(0xE7000000, 0);
    }
    {
    s32 line_y;
    func_80200F70(D_801F36C0 + 0x320, D_801F0B90 + 0x2828, 48, 32, 2);
    line_y = y + 24;
    func_80200DD0(x + 68, line_y, 30);
    func_80200DD0(x + 92, line_y, 31);
    func_80200DD0(x + 126, y + 40, 26);
    COMMAND(0xDE000000, D_80213648);
    COMMAND(0xE3001201, 0);
    func_80200F70(D_80212318, D_80211DD0, 48, 56, 2);
    func_80200DD0(x + 10, line_y, 44);
    }
    if (D_80219D24 == 4) {
        s32 counter = D_80214F4C;
        s32 offset = counter < 4 ? counter : 7 - counter;
        func_80201384(W32(0x2C) - 16, W32(0x80), ((ShopWindowWidthView *)window)->field34, W32(0x88));
        func_801818A0(x + offset - 11, y + 8);
    }
}





