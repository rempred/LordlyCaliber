#include "game/shop_price_interface.h"

typedef struct { u32 w0; u32 w1; } ShopGfx;
/* Minimum ten-byte row storage view; consumers retain signed halfwords. */
typedef struct {
    u16 field00, field02, field04, field06, field08;
} ShopRow;

extern u32 *volatile D_800E9BA0;
extern ShopRow *D_80219F40;
extern u8 D_8021A121, D_8021A122;
extern u8 D_80219D24, D_80219D25, D_80219D26;
extern s32 D_80214F4C;
extern u32 D_8021A0D8[], D_8021A010[];
extern s8 D_8021A11A[];
extern u16 D_8018E6D2[][6], D_8018C414[][16];
extern void *D_8021A128;
extern u8 *D_801F36C0, *D_801F0B90;
extern u8 D_80210AA8[], D_80210A20[], D_80213648[], D_80210258[];
extern u8 D_80211530[], D_80211558[], D_80211578[], D_80211BC8[];
extern u8 D_802115A0[], D_80212318[], D_80211DD0[];

extern u8 func_0019B21C(u8 category);
extern void func_0019AF78(s32 x, s32 y, s32 item, u32 slot);
extern void func_8017C4B4(s32 value);
extern void func_80201384(s32 a, s32 b, s32 c, s32 d);
extern void func_80200F70(void *a, void *b, s32 width, s32 height, s32 mode);
extern void func_80200DD0(s32 x, s32 y, s32 size);
extern void func_802012D8(void *data);
extern void func_8020122C(void *data);
extern void func_8020E7F4(s32 x, s32 width, s32 y0, s32 y1, u32 alpha);
extern void func_80204050(s32 value);
extern void func_8017A704(void);
extern void func_8017A714(void);
extern void *func_8016F4E0(u32 item);
extern void *func_8016F5B0(u32 item);
extern void func_80179034(void *text, s32 x, s32 y);
extern void func_801818A0(s32 x, s32 y);
extern void func_8020E0F0(s32 x, s32 y, u32 value, s32 digits);
extern void func_8017D6D0(void *window, u32 a, u32 b, u32 c,
                       s32 width, s32 y, s32 height);

#define W32(off) (*(s32 *)(window + (off)))
#define W16(off) (*(u16 *)(window + (off)))
#define VISIBLE() (*(s16 *)(window + 0x18 + slot * 2))
#define ALPHA() (*(u8 *)(window + 0x19 + slot * 2))
#define ROW() ((ShopRow *)((u32)D_8021A121 * sizeof(ShopRow) + (u32)D_80219F40))
#define ROW_BYTE(p, off) (((u8 *)(p))[off])
#define COMMAND(a, b) { \
    ShopGfx *g = (ShopGfx *)D_800E9BA0; \
    D_800E9BA0 = (u32 *)(g + 1); \
    g->w0 = (u32)(a); g->w1 = (u32)(b); \
}
/* Preserve the original signed-halfword clipping masks. */
#define CLIP4(v) (((u32)(v) << 2) & \
                  (u32)(~(s32)(s16)((u32)(v) << 2) >> 31) & 0xFFC)

void func_001989C4(u8 *window, s32 slot)
{
    s32 y_base, row_count, row, vertical, x, y, item_index;
    ShopRow *info;
    s32 counter, pulse;
    u32 setup_mode;

    if (VISIBLE() == 0) {
        return;
    }

    counter = D_80214F4C + 1;
    D_80214F4C = counter;
    D_80214F4C = counter % 8;
    pulse = D_80219D25;
    if ((pulse & 0x7F) >= 31) {
        D_80219D25 = (D_80219D25 & 0x80) ^ 0x80;
    }
    D_80219D25++;
    if (D_80219D25 & 0x80) {
        D_80219D26 += 2;
    } else {
        D_80219D26 -= 2;
    }
    y_base = W32(0x30);
    {
        ShopGfx *g = (ShopGfx *)D_800E9BA0;
        g->w0 = 0xE7000000;
        g->w1 = 0;
        setup_mode = W16(0x28);
        D_800E9BA0 = (u32 *)(g + 1);
    }
    if (setup_mode != 3) {
        func_8017C4B4(VISIBLE());
    } else {
        func_80201384(W32(0x7C), W32(0x80), W32(0x84), W32(0x88));
    }

    row_count = ROW_BYTE(ROW(), 5);
    row = 0;
    if (row < row_count) {
    vertical = 0;
    for (; row < row_count; vertical += 16, row++) {
        u32 item;
        void *text;
        s32 offset;
        s32 field02_word, field08_word, field06_word, field00_word;
        info = ROW();
        y = y_base + row * 16;
        x = W32(0x2C);
        /* Keep the real storage reads and signed word consumers separate.
         * The pinned loop pass sees these conversions before they fold to LH;
         * their lifetimes keep the slot-address calculation at its consumer. */
        field02_word = info->field02;
        field02_word = (s16)field02_word;
        item_index = row + field02_word;
        field08_word = info->field08;
        field08_word = (s16)field08_word;
        if (field08_word == item_index) {
            x += 4;
        }
        field06_word = info->field06;
        field06_word = (s16)field06_word;
        if (field06_word < item_index) {
            continue;
        }
        COMMAND(0xDE000000, D_80213648);
        func_80200F70(D_80210AA8, D_80210A20, 16, 16, 2);
        func_80200DD0(x + 4, y + 21, 19);
        COMMAND(0xDE000000, D_80213648);
        func_802012D8(D_80210258);
        {
            /* Capture the head before materializing the consumed sync word.
             * The explicit row consumers retain the neighboring local lifetimes. */
            u32 sync_opcode;
            ShopGfx *g = (ShopGfx *)D_800E9BA0;
            sync_opcode = 0xE7000000;
            D_800E9BA0 = (u32 *)(g + 1);
            g->w0 = sync_opcode;
            g->w1 = 0;
        }
        field00_word = ROW()->field00;
        field00_word = (s16)field00_word;
        if (field00_word == item_index && D_80219D24 >= 3) {
            func_8020E7F4(W32(0x2C), W32(0x34), y + 20, y + 36, D_80219D26);
            if (W16(0x28) != 3) {
                func_80204050(VISIBLE());
            }
            func_8017A704();
            func_802012D8(D_80211530);
        }
        if (D_8021A121 == D_8021A122) {
            item = (u16)D_8021A0D8[item_index];
            text = func_8016F4E0(item);
        } else {
            item = (u16)D_8021A010[item_index + func_0019B21C(D_8021A121)];
            text = func_8016F5B0(item);
        }
        func_80179034(text, x + 20, y + 21);
        func_8017A714();
        item &= 0xFFFF;
        if (item != 0) {
            COMMAND(0xE3001001, 0x8000);
            func_0019AF78(x + 4, y + 20, item,
                         (u8)(func_0019B21C(D_8021A121) + item_index));
        }
        if (((s16)ROW()->field00) != item_index) {
            continue;
        }
        if (D_80219D24 == 0 || D_80219D24 == 3 || D_80219D24 == 14) {
            offset = 0;
            if (D_80219D24 == 3) {
                s32 blink = D_80214F4C;
                if (blink < 4) {
                    offset = blink;
                } else {
                    offset = 7 - blink;
                }
            }
            func_80201384(W32(0x2C) - 16, W32(0x80), W32(0x34), W32(0x88));
            func_801818A0(x + offset - 12, vertical + y_base + 23);
        }
    }

    }
    COMMAND(0xDE000000, D_80213648);
    func_80200F70(D_80211558, D_8021A128, 80, 17, 2);
    {
    s32 price_y;
    u32 *equipment_items, *consumable_items;
    row = 0;
    if (row < row_count) {
    equipment_items = D_8021A0D8;
    consumable_items = D_8021A010;
    price_y = y_base;
    for (; row < row_count; row++, price_y += 16) {
        u32 value, item;
        ShopRow *price_info;
        price_info = ROW();
        x = W32(0x2C);
        item_index = row + ((s16)price_info->field02);
        if (((s16)price_info->field08) == item_index) {
            x += 4;
        }
        if (((s16)price_info->field06) < item_index) {
            continue;
        }
        if ((((s16)price_info->field00) == item_index || ((s16)price_info->field08) == item_index) && D_80219D24 >= 3) {
            func_802012D8(D_80211578);
        } else {
            func_802012D8(D_80211558);
        }
        if (D_8021A121 == D_8021A122) {
            item = equipment_items[item_index];
            value = D_8018E6D2[item][0];
        } else {
            item = consumable_items[item_index + func_0019B21C(D_8021A121)];
            if (item == 250) {
                value = func_0019BD14();
            } else {
                value = D_8018C414[item][0];
            }
        }
        func_8020E0F0(x + 111, price_y + 25, value, -5);
    }

    }
    }
    x = W32(0x2C);
    COMMAND(0xDE000000, D_80213648);
    func_8020122C(D_80211BC8);
    row = 0;
    {
        u32 load_block = 0x0730F093;
        u32 tile = 0xF5481C00;
        ShopGfx *commands = (ShopGfx *)D_800E9BA0;
        u32 size = 0x001BC034;
        s32 last = D_8021A122;
        D_800E9BA0 = (u32 *)(commands + 1);
        commands[0].w0 = 0xFD500000;
        commands[0].w1 = (u32)D_802115A0;
        D_800E9BA0 = (u32 *)(commands + 2);
        commands[1].w0 = 0xF5500000;
        commands[1].w1 = 0x07000000;
        D_800E9BA0 = (u32 *)(commands + 3);
        commands[2].w0 = 0xE6000000;
        commands[2].w1 = 0;
        D_800E9BA0 = (u32 *)(commands + 4);
        commands[3].w0 = 0xF3000000;
        commands[3].w1 = load_block;
        D_800E9BA0 = (u32 *)(commands + 5);
        commands[4].w0 = 0xE7000000;
        commands[4].w1 = 0;
        D_800E9BA0 = (u32 *)(commands + 6);
        commands[5].w0 = tile;
        commands[5].w1 = 0;
        D_800E9BA0 = (u32 *)(commands + 7);
        commands[6].w0 = 0xF2000000;
        commands[6].w1 = size;
        if (row <= last) {
            u32 clip_sync = 0xE7000000;
            s32 clip_limit, wrapped_y;
            u32 upper_y, scaled_y;
            u8 *slot_window;
            /* The row index is dead after the price pass; reuse its value
             * slot for the selected category during command generation. */
            item_index = D_8021A121;
            slot_window = (u8 *)((u32)slot * 2 + (u32)window);
            upper_y = CLIP4(y_base + 19) | 0xE4000000;
            do {
                u32 lower_y = CLIP4(y_base + 5);
                ShopGfx *g = (ShopGfx *)D_800E9BA0;
                ShopGfx *texture_command;
                u32 shade = ((0u - ((u32)item_index == (u32)row)) & 0xFF) | 0x60;
                s32 step = row * 15 + 4;
                s32 left = x + step;
                s32 wrapped_x = (s16)((u32)left << 2);
                u32 texture_word;
                wrapped_y = (s16)((u32)(y_base + 5) << 2);
                scaled_y = (u32)wrapped_y << 10;
                clip_limit = last;
                D_800E9BA0 = (u32 *)(g + 1);
                g[0].w0 = 0xFA000000;
                g[0].w1 = (shade << 24) | (shade << 16) | (shade << 8) | slot_window[0x19];
                D_800E9BA0 = (u32 *)(g + 2);
                g[1].w0 = clip_sync;
                g[1].w1 = 0;
                D_800E9BA0 = (u32 *)(g + 3);
                g[2].w0 = (CLIP4(left + 14) << 12) | upper_y;
                g[2].w1 = (CLIP4(left) << 12) | lower_y;
                texture_command = g + 3;
                D_800E9BA0 = (u32 *)(g + 4);
                texture_command->w0 = 0xE1000000;
                texture_word = (u32)(s32)D_8021A11A[row] << 9;
                if (wrapped_x < 0) {
                    s32 scaled = wrapped_x * 8;
                    texture_word = (texture_word - (scaled & -(scaled < 1))) << 16;
                } else {
                    texture_word = (u32)(u16)texture_word << 16;
                }
                texture_command->w1 = texture_word;
                if ((s32)((u32)(y_base + 5) << 2) < 0) {
                    s32 scaled = (s32)scaled_y >> 7;
                    s32 nonpositive = scaled < 1;
                    /* Keep these genuine value updates: the pinned loop pass
                     * otherwise hoists and folds the conditional correction. */
                    nonpositive = -nonpositive;
                    scaled &= nonpositive;
                    scaled = -scaled;
                    texture_command->w1 = texture_word | (u16)scaled;
                }
                {
                    ShopGfx *tail = (ShopGfx *)D_800E9BA0;
                    D_800E9BA0 = (u32 *)(tail + 1);
                    tail[0].w0 = 0xF1000000;
                    tail[0].w1 = 0x04000400;
                    D_800E9BA0 = (u32 *)(tail + 2);
                    tail[1].w0 = clip_sync;
                    tail[1].w1 = 0;
                }
                row++;
            } while (row <= clip_limit);
        }
    }
    COMMAND(0xFA000000, 0xFFFFFF00 | ALPHA());
    func_80201384(W32(0x2C), W32(0x30) - 8, W32(0x34) + 8, W32(0x38));
    {
        u32 mode_word = 0xE3001201;
        ShopGfx *tail = (ShopGfx *)D_800E9BA0;
        D_800E9BA0 = (u32 *)(tail + 1);
        tail[0].w0 = 0xDE000000;
        tail[0].w1 = (u32)D_80213648;
        D_800E9BA0 = (u32 *)(tail + 2);
        tail[1].w0 = mode_word;
        tail[1].w1 = 0;
    }
    func_80200F70(D_80212318, D_80211DD0, 48, 56, 2);
    func_80200DD0(x + 32, y_base - 3, 42);
    func_80200F70(D_801F36C0 + 0x320, D_801F0B90 + 0x2828, 48, 32, 2);
    func_80200DD0(x + 115, y_base - 3, 35);
    if (D_80219D24 == 3) {
        info = ROW();
        if (((s16)info->field04) <= ((s16)info->field06)) {
            func_8017D6D0(window, ROW_BYTE(info, 3), ((s16)info->field04) & 0xFF,
                          ((s16)info->field06) & 0xFF, W32(0x34) - 4,
                          W32(0x30) + 24, W32(0x38) - 12);
        }
    }
}
