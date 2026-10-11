#include "common/types.h"

typedef struct { u32 w0, w1; } ShopGfx;
typedef struct { s16 field00, field02, field04, field06, field08; } ShopRow;
typedef s8 (*ShopCallback)(u32 item);
extern u32 *D_800E9BA0;
extern ShopRow *D_80219F40;
extern u8 D_8021A121, D_8021A122;
extern u32 D_8021A0D8[], D_8021A010[];
extern void *D_80213108[], *D_8021311C[], *D_80213130[], *D_802130A0[];
extern void *D_80213140[], *D_80187BD4[];
extern u8 D_80219BF0[], D_80219BF8[], D_80219C04[], D_80219C0C[];
extern u8 D_80219DB0[], D_80219DB4[];
extern ShopCallback D_80219D28[], D_80219D40[];
extern u8 *D_801F36C0, *D_801F0B90;
extern u8 D_80210258[], D_80210298[], D_802102B8[], D_80212948[], D_802129D0[];
extern u8 func_0019B21C(u8 category);
extern u8 func_8016F500(u32 item), func_8016F540(u32 item);
extern u8 func_8016F5C8(u32 item), func_8016F5E0(u32 item);
extern u32 func_8016F580(u32 item), func_8016F598(u32 item);
extern s32 func_8017B58C(void *text);
extern void func_8017C4B4(s32 value);
extern void func_80179034(void *text, s32 x, s32 y);
extern void func_8009C550(void *dest, void *source);
extern void func_8009C620(void *dest, void *source);
extern s32 func_80093460(void *text);
extern void func_802027DC(void);
extern void func_802012D8(void *data);
extern void func_802001A8(s32 x, s32 y, s32 value, s32 digits);
extern void func_80200F70(void *a, void *b, s32 width, s32 height, s32 mode);
extern void func_80200DD0(s32 x, s32 y, s32 size);

#define ROW() (&D_80219F40[D_8021A121])
#define LOW_WORD(a,i) (*(u16 *)((u8 *)&(a)[i] + 2))
#define COMMAND(a,b) { \
    ShopGfx *g = (ShopGfx *)D_800E9BA0; \
    D_800E9BA0 = (u32 *)(g + 1); \
    g->w0 = (u32)(a); g->w1 = (u32)(b); \
}
#define COMMAND_WORK_PHASE(opcode,data,work) { \
    ShopGfx *local_tail = (ShopGfx *)D_800E9BA0; \
    (work) = (u32)(local_tail + 1); \
    D_800E9BA0 = (u32 *)(work); \
    (work) = (u32)(opcode); \
    local_tail->w0 = (work); local_tail->w1 = (u32)(data); \
}
#define CLIP4(v) (((u32)(v) << 2) & \
                  (u32)(~(s32)(s16)((u32)(v) << 2) >> 31) & 0xFFC)
/* Each use expands ordinary C for one observed three-command rectangle.
 * Coordinates retain the signed-halfword clipping seen in the original. */
#define RECT_GRID_FIELDS(lx,ly,tx,ty,sync,word0,base_x,base_y,grid_row) { \
    ShopGfx *g = (ShopGfx *)D_800E9BA0; \
    ShopGfx *texture = g + 2; \
    u32 *texture_word_address; \
    s32 wrapped_x, origin_y, row_offset_x, row_offset_y; \
    u32 texture_word, texture_y, texture_y_field, upper_y_word; \
    row_offset_x = ((grid_row) / 3) * 54 + 4; \
    (lx) = (base_x) + row_offset_x; \
    (word0) = CLIP4((lx) + 21) << 12; \
    row_offset_y = ((grid_row) % 3) * 10 + 44; \
    (ly) = (base_y) + row_offset_y; \
    upper_y_word = CLIP4((ly) + 8); \
    upper_y_word |= 0xE4000000; \
    (word0) |= upper_y_word; \
    D_800E9BA0 = (u32 *)(g + 1); \
    g[1].w0 = (word0); \
    D_800E9BA0 = (u32 *)(g + 2); \
    wrapped_x = (s16)((u32)(lx) << 2); \
    g[1].w1 = (CLIP4(lx) << 12) | CLIP4(ly); \
    D_800E9BA0 = (u32 *)(g + 3); \
    g[0].w0 = (sync); g[0].w1 = 0; \
    texture->w0 = 0xE1000000; \
    texture_y = (u32)(ty); \
    if (wrapped_x < 0) { \
        s32 scaled = wrapped_x * 8; \
        s32 nonpositive = scaled < 1; \
        nonpositive = -nonpositive; scaled &= nonpositive; \
        texture_word = ((u32)(tx) - scaled) << 16; \
    } else { texture_word = (u32)(u16)(tx) << 16; } \
    origin_y = (base_y) + 44 + ((grid_row) % 3) * 10; \
    if ((s32)((u32)origin_y << 2) < 0) { \
        s32 scaled = (s32)((u32)origin_y << 18) >> 13; \
        s32 nonpositive = scaled < 1; \
        nonpositive = -nonpositive; scaled &= nonpositive; \
        texture_y_field = (u16)(texture_y - scaled); \
    } else { texture_y_field = (u16)texture_y; } \
    texture_word_address = (u32 *)texture + 1; \
    *texture_word_address = texture_word | texture_y_field; \
}
#define RECT_SECOND_PHASES(lx,ly,ry,tx,ty,sync,base_y,step,base_x,word0,word1) { \
    s32 origin_y; \
    u32 upper_geometry_word, lower_geometry_word; \
    u32 texture_y_field, lower_y_word; \
    u32 *texture_word_address; \
    head_word = (u32)D_800E9BA0; \
    presence_or_scan_draw_word = (u32)(((ShopGfx *)head_word) + 1); \
    texture = ((ShopGfx *)head_word) + 2; \
    D_800E9BA0 = (u32 *)presence_or_scan_draw_word; \
    ((ShopGfx *)head_word)[0].w0 = (sync); ((ShopGfx *)head_word)[0].w1 = 0; \
    D_800E9BA0 = (u32 *)texture; \
    presence_or_scan_draw_word = (base_x) + 68; \
    upper_geometry_word = presence_or_scan_draw_word << 2; \
    presence_or_scan_draw_word <<= 18; \
    presence_or_scan_draw_word = (u32)((s32)presence_or_scan_draw_word >> 16); \
    presence_or_scan_draw_word = ~presence_or_scan_draw_word; \
    presence_or_scan_draw_word = (u32)((s32)presence_or_scan_draw_word >> 31); \
    upper_geometry_word &= presence_or_scan_draw_word; \
    upper_geometry_word &= 0xFFC; \
    upper_geometry_word <<= 12; \
    presence_or_scan_draw_word = (ry); \
    upper_y_word = CLIP4(presence_or_scan_draw_word); \
    presence_or_scan_draw_word = 0xE4000000; \
    upper_y_word |= presence_or_scan_draw_word; \
    upper_geometry_word |= upper_y_word; \
    ((ShopGfx *)head_word)[1].w0 = upper_geometry_word; \
    presence_or_scan_draw_word = (base_x) + 36; \
    lower_geometry_word = presence_or_scan_draw_word << 2; \
    presence_or_scan_draw_word <<= 18; \
    upper_geometry_word = (u32)((s32)presence_or_scan_draw_word >> 16); \
    presence_or_scan_draw_word = ~upper_geometry_word; \
    presence_or_scan_draw_word = (u32)((s32)presence_or_scan_draw_word >> 31); \
    lower_geometry_word &= presence_or_scan_draw_word; \
    lower_geometry_word &= 0xFFC; \
    lower_geometry_word <<= 12; \
    presence_or_scan_draw_word = (u32)(ly) << 18; \
    presence_or_scan_draw_word = (u32)((s32)presence_or_scan_draw_word >> 16); \
    presence_or_scan_draw_word = ~presence_or_scan_draw_word; \
    presence_or_scan_draw_word = (u32)((s32)presence_or_scan_draw_word >> 31); \
    lower_y_word = (u32)(ly) << 2; \
    lower_y_word &= presence_or_scan_draw_word; \
    lower_y_word &= 0xFFC; \
    lower_geometry_word |= lower_y_word; \
    ((ShopGfx *)head_word)[1].w1 = lower_geometry_word; \
    presence_or_scan_draw_word = (u32)(((ShopGfx *)head_word) + 3); \
    D_800E9BA0 = (u32 *)presence_or_scan_draw_word; \
    presence_or_scan_draw_word = 0xE1000000; \
    ((ShopGfx *)head_word)[2].w0 = presence_or_scan_draw_word; \
    head_word = (u32)(ty); \
    if ((s32)upper_geometry_word < 0) { \
        s32 nonpositive; \
        lower_geometry_word = (u32)upper_geometry_word << 3; \
        nonpositive = (s32)lower_geometry_word < 1; \
        nonpositive = -nonpositive; lower_geometry_word &= nonpositive; \
        lower_geometry_word = (u32)(tx) - lower_geometry_word; \
        upper_geometry_word = lower_geometry_word << 16; \
    } else { upper_geometry_word = (u32)(u16)(tx) << 16; } \
    origin_y = (base_y) + (step); \
    texture_word_address = (u32 *)texture + 1; \
    if ((s32)((u32)origin_y << 2) < 0) { \
        s32 scaled = (s32)((u32)origin_y << 18) >> 13; \
        s32 nonpositive = scaled < 1; \
        nonpositive = -nonpositive; scaled &= nonpositive; \
        texture_y_field = (u16)(head_word - scaled); \
        *texture_word_address = (u32)upper_geometry_word | texture_y_field; \
    } else { \
        texture_y_field = (u16)head_word; \
        *texture_word_address = (u32)upper_geometry_word | texture_y_field; \
    } \
    upper_geometry_word = 0x04000400; \
    (step) += 10; \
    COMMAND_WORK_PHASE(0xF1000000, upper_geometry_word, presence_or_scan_draw_word); \
}
#define RECT_PATCH_PHASES(lx,ly,base_x,ry,tx,sync,upper_x,word0,word1,work_y,base_y,step) { \
    s32 wrapped_x; \
    u32 geometry_word; \
    head_word = (u32)D_800E9BA0; \
    (ly) = (u32)((base_y) + (step)); \
    presence_or_scan_draw_word = (u32)(((ShopGfx *)head_word) + 1); \
    texture = ((ShopGfx *)head_word) + 2; \
    D_800E9BA0 = (u32 *)presence_or_scan_draw_word; \
    ((ShopGfx *)head_word)[0].w0 = (sync); ((ShopGfx *)head_word)[0].w1 = 0; \
    D_800E9BA0 = (u32 *)texture; \
    presence_or_scan_draw_word = (ly) + 8; \
    upper_y_word = presence_or_scan_draw_word << 2; \
    presence_or_scan_draw_word <<= 18; \
    presence_or_scan_draw_word = (u32)((s32)presence_or_scan_draw_word >> 16); \
    presence_or_scan_draw_word = ~presence_or_scan_draw_word; \
    presence_or_scan_draw_word = (u32)((s32)presence_or_scan_draw_word >> 31); \
    upper_y_word &= presence_or_scan_draw_word; \
    upper_y_word &= 0xFFC; \
    presence_or_scan_draw_word = 0xE4000000; \
    upper_y_word |= presence_or_scan_draw_word; \
    geometry_word = (upper_x); \
    geometry_word |= upper_y_word; \
    ((ShopGfx *)head_word)[1].w0 = geometry_word; \
    presence_or_scan_draw_word = (base_x) + 4; \
    geometry_word = presence_or_scan_draw_word << 2; \
    presence_or_scan_draw_word <<= 18; \
    wrapped_x = (s32)presence_or_scan_draw_word >> 16; \
    presence_or_scan_draw_word = (u32)~wrapped_x; \
    presence_or_scan_draw_word = (u32)((s32)presence_or_scan_draw_word >> 31); \
    geometry_word &= presence_or_scan_draw_word; \
    geometry_word &= 0xFFC; \
    geometry_word <<= 12; \
    presence_or_scan_draw_word = (u32)(ly) << 2; \
    (ly) = (u32)(ly) << 18; \
    (ly) >>= 16; \
    (ly) = ~(ly); \
    (ly) >>= 31; \
    presence_or_scan_draw_word &= (u32)(ly); \
    presence_or_scan_draw_word &= 0xFFC; \
    geometry_word |= presence_or_scan_draw_word; \
    ((ShopGfx *)head_word)[1].w1 = geometry_word; \
    presence_or_scan_draw_word = (u32)(((ShopGfx *)head_word) + 3); \
    D_800E9BA0 = (u32 *)presence_or_scan_draw_word; \
    presence_or_scan_draw_word = 0xE1000000; \
    texture->w0 = presence_or_scan_draw_word; \
    (ly) = (u32)(tx); \
    if (wrapped_x < 0) { \
        s32 nonpositive; \
        geometry_word = (u32)wrapped_x << 3; \
        nonpositive = (s32)geometry_word < 1; \
        nonpositive = -nonpositive; geometry_word &= nonpositive; \
        geometry_word = (u32)(u16)(ly) - geometry_word; \
        (ly) = geometry_word << 16; \
    } else { (ly) = (u32)(u16)(ly) << 16; } \
    texture->w1 = (u32)(ly); \
    (work_y) = (base_y) + (step); \
    if ((s32)((u32)(work_y) << 2) < 0) { \
        s32 scaled = (s32)((u32)(work_y) << 18) >> 13; \
        s32 nonpositive = scaled < 1; \
        nonpositive = -nonpositive; scaled &= nonpositive; \
        scaled = -scaled; texture->w1 = (u32)(ly) | (u16)scaled; \
    } \
    wrapped_x = 0x04000400; \
    (ly) = (base_x); \
    (ly) += 16; \
    COMMAND_WORK_PHASE(0xF1000000, wrapped_x, presence_or_scan_draw_word); \
}

void func_001994B4(u8 *window, u32 slot_word)
{
    s32 x, y;
    s32 row;
    u32 item, argument_item;
    u32 cached_loop_sync, flag_grid_or_vertical_word, byte_or_final_sync_word, first_byte_or_texture_y_word;
    u32 presence_or_scan_draw_word;
    char buffer[64];
    slot_word = (slot_word << 1) + (u32)window;
    if (*(s16 *)(slot_word + 0x18) == 0) { return; }
    /* This scratch first transports the X field word, then retains Y. */
    y = *(s32 *)(window + 0x2C);
    x = y;
    y = *(s32 *)(window + 0x30);
    {
        u32 sync = 0xE7000000;
        ShopGfx *g = (ShopGfx *)D_800E9BA0;
        s32 value;
        g->w0 = sync; g->w1 = 0;
        value = *(s16 *)(slot_word + 0x18);
        D_800E9BA0 = (u32 *)(g + 1);
        func_8017C4B4(value);
    }
    cached_loop_sync = 0xE7000000;
    if (D_8021A121 == D_8021A122) {
        s32 left, label_x;
        u32 equipment_item;
        equipment_item = (u16)D_8021A0D8[ROW()->field00];
        left = x + 4;
        func_80179034(D_80213108[func_8016F500(equipment_item)], left, y + 4);
        label_x = x + 32;
        func_80179034(D_8021311C[func_8016F500(equipment_item)], label_x, y + 17);
        func_80179034(D_80219BF0, left, y + 34);
        func_80179034(D_80213130[func_8016F540(equipment_item)], label_x, y + 47);
        return;
    }
    {
        u32 category = func_0019B21C(D_8021A121);
        void *text;
        s32 width;
        /* The retail lookup keeps the table address and loaded item in
         * the same word register; retain the original word-object view. */
        item = (u32)&D_8021A010[ROW()->field00 + category];
        item = (u16)*(u32 *)item;
        argument_item = item & 0xFFFFu;
        text = D_802130A0[func_8016F5C8(argument_item)];
        width = func_8017B58C(text);
        width /= 2;
        width -= 52;
        func_80179034(text, x - width, y + 2);
    }
    {
        u32 second_flags;
        s32 flag_index;
        char *tail;
        flag_grid_or_vertical_word = func_8016F580(argument_item);
        flag_grid_or_vertical_word &= 14;
        second_flags = func_8016F598(argument_item) & 14;
        first_byte_or_texture_y_word = (u8)flag_grid_or_vertical_word;
        presence_or_scan_draw_word = first_byte_or_texture_y_word != 0;
        byte_or_final_sync_word = (u8)second_flags;
        flag_index = byte_or_final_sync_word != 0;
        presence_or_scan_draw_word |= flag_index;
        if (presence_or_scan_draw_word) {
            s32 length;
            tail = buffer + 1;
            buffer[0] = 14;
            func_8009C550(tail, D_80219BF8);
            func_8009C620(buffer, D_80219DB0);
            func_8009C620(buffer, D_80187BD4[func_8016F5E0(argument_item)]);
            length = func_80093460(buffer);
            buffer[length] = 15;
            tail[length] = 0;
            func_80179034(buffer, x + 2, y + 16);
            flag_index = 0;
            {
                void *suffix;
                char *append_base;
                char *tail;
                char *delimiter;
                if (first_byte_or_texture_y_word != 0) {
                    do {
                        presence_or_scan_draw_word = flag_grid_or_vertical_word >> 1;
                        flag_grid_or_vertical_word = presence_or_scan_draw_word;
                        presence_or_scan_draw_word &= 1;
                        flag_index++;
                    } while (!presence_or_scan_draw_word);
                    buffer[0] = 14;
                    tail = buffer + 1;
                    func_8009C550(tail, D_80213140[flag_index]);
                    func_8009C620(buffer, D_80219DB4);
                    append_base = buffer;
                    suffix = D_80219C04;
                } else if (byte_or_final_sync_word != 0) {
                    u32 shifted_flags;
                    do {
                        shifted_flags = second_flags >> 1;
                        second_flags = shifted_flags;
                        shifted_flags &= 1;
                        flag_index++;
                    } while (!shifted_flags);
                    buffer[0] = 14;
                    tail = buffer + 1;
                    func_8009C550(tail, D_80213140[flag_index]);
                    func_8009C620(buffer, D_80219DB4);
                    append_base = buffer;
                    suffix = D_80219C0C;
                } else {
                    goto display_secondary_text;
                }
                func_8009C620(append_base, suffix);
                length = func_80093460(buffer);
                delimiter = buffer;
                delimiter += length;
                flag_index = 15;
                tail += length;
                *delimiter = (u8)flag_index;
                *tail = 0;
            }
        display_secondary_text:
            func_80179034(buffer, x + 8, y + 29);
        } else {
            func_80179034(D_80219BF8, x + 4, y + 16);
            func_80179034(D_80187BD4[func_8016F5E0(argument_item)], x + 32, y + 29);
        }
    }
    func_802027DC();
    {

        u32 consumer_offset;
        s32 vertical;
        row = 0;
        item &= 0xFFFFu;
        vertical = 80;
        consumer_offset = 0;
        /* The first flags, callback cursor, grid sync, and final vertical
         * occupy disjoint phases of this 32-bit scratch word. */
        flag_grid_or_vertical_word = (u32)D_80219D28;
        for (; row < 7; consumer_offset += 4, vertical += 10, row++, flag_grid_or_vertical_word += sizeof(ShopCallback)) {
            s32 first = (*(ShopCallback *)flag_grid_or_vertical_word)(item);
            s32 second = (*(ShopCallback *)((u8 *)D_80219D40 + consumer_offset))(item);
            if (row < 6) {
                {
                void *resource;
                if (first > 0) { resource = D_80210298; }
                else {
                    resource = D_80210258;
                    if (first < 0) { resource = D_802102B8; }
                }
                func_802012D8(resource);
            }
                func_802001A8(x + 20 + (row / 3) * 54, y + 44 + (row % 3) * 10, first, -4);
            }
            {
                void *resource;
                if (second > 0) { resource = D_80210298; }
                else {
                    resource = D_80210258;
                    if (second < 0) { resource = D_802102B8; }
                }
                func_802012D8(resource);
            }
            func_802001A8(x + 74, y + vertical, second, -4);
        }
    }
    {
        u32 setup_sync = 0xE7000000;
        COMMAND(setup_sync, 0);
        COMMAND(0xFD100000, D_80210258);
        COMMAND(0xE8000000, 0);
        COMMAND(0xF5000100, 0x07000000);
        COMMAND(0xE6000000, 0);
        COMMAND(0xF0000000, 0x0703C000);
        COMMAND(setup_sync, 0);
        COMMAND(0xE3001001, 0x8000);
        COMMAND(0xD7000002, 0x80008000);
    }
    func_80200F70(D_801F36C0 + 0x3A0, D_801F0B90 + 0x2F48, 32, 48, 2);
    {
        row = 0;
        flag_grid_or_vertical_word = 0xE7000000;
        for (; row < 7; row++) {
            s32 vertical = row * 10 + 80;
            if (row < 6) {
                s32 rectangle_x, rectangle_y;
                u32 grid_word;
                RECT_GRID_FIELDS(rectangle_x, rectangle_y,
                           0, (u32)row << 8, flag_grid_or_vertical_word, grid_word, x, y, row);
                COMMAND(0xF1000000, 0x04000400);
                COMMAND(flag_grid_or_vertical_word, 0);
            }
            func_80200DD0(x + 72, y + vertical, 38);
        }
    }
    {
        u32 mode_word = 0xE3001001;
        u32 render_word = 0xD7000002;
        u32 render_argument = 0x80008000;
        u32 load_block_word = 0x0723F400;
        u32 tile_word = 0xF5400400;
        u32 tile_size_word = 0x0007C11C;
        u32 setup_sync = 0xE7000000;

        u32 upper_x;

        row = 0;
        byte_or_final_sync_word = cached_loop_sync;
        upper_x = CLIP4(x + 12) << 12;
        COMMAND(setup_sync, 0);
        COMMAND(mode_word, 0x8000);
        COMMAND(render_word, render_argument);
        COMMAND(0xFD500000, D_802129D0);
        COMMAND(0xF5500000, 0x07000000);
        COMMAND(0xE6000000, 0);
        COMMAND(0xF3000000, load_block_word);
        COMMAND(setup_sync, 0);
        COMMAND(tile_word, 0);
        COMMAND(0xF2000000, tile_size_word);
        flag_grid_or_vertical_word = 80;
        first_byte_or_texture_y_word = 512;
        for (; row < 7; row++) {
            s32 rectangle_x, rectangle_y, geometry_y;
            u32 parity;
            ShopGfx *texture;
            u32 head_word, upper_y_word;
            parity = row & 1;
            if (parity == 0) { func_802012D8(D_80212948 + row * 16); }
            RECT_PATCH_PHASES(rectangle_x, geometry_y, x, geometry_y + 8,
                       parity << 8, byte_or_final_sync_word, upper_x, 0, 0, rectangle_y, y, flag_grid_or_vertical_word);
            COMMAND(byte_or_final_sync_word, 0);
            COMMAND(byte_or_final_sync_word, 0);
            func_80200DD0(geometry_y, rectangle_y, 74);

            RECT_SECOND_PHASES(rectangle_x, rectangle_y, rectangle_y + 8,
                       0, first_byte_or_texture_y_word, byte_or_final_sync_word, y, flag_grid_or_vertical_word, x, 0, 0);
            COMMAND(byte_or_final_sync_word, 0);
            COMMAND(byte_or_final_sync_word, 0);
            first_byte_or_texture_y_word += 256;
        }
    }
}

