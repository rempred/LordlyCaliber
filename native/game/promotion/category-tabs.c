/* Modified List Classes tab strip. The Army texture pack carries the
 * class-to-tab map and independent 16x14 CI8 icon cells. Selection still uses
 * the original left/right controller; render eight tabs around that selection.
 */
#include "class_menu_config.h"
typedef unsigned char u8;
typedef unsigned int u32;
extern u8 *volatile lc_army_owner;
extern u32 *lc_gfx;
void lc_load_tab(const u8 *palette, const u8 *pixels, u32 width, u32 height);

void lc_category_tabs(int x, int y, int alpha) {
    u8 *owner = lc_army_owner;
    const u8 *data = *(u8 **)(owner + 4) + 23496;
    const u8 *palette = *(u8 **)(owner + 8) + 0x5E0;
    u32 selected = owner[0x9D], count = owner[0x1DFF];
    u32 first = (selected / 8) * 8, index;
    u32 *g = lc_gfx;
    g[0] = 0xFC119623;
    g[1] = 0xFF2FFFFF;
    g[2] = 0xDE000000;
    g[3] = 0x060047A0;
    lc_gfx = g + 4;
    y += 7;
    for (index = first; index < count && index < first + 8; ++index) {
        u32 category = owner[LC_CATEGORY_TABS_OFFSET + index];
        u32 icon = data[264 + category];
        u32 color = index == selected ? 0xFFFFFF00 : 0x60606000;
        int px = x + 4 + (index - first) * 15;
        u32 left, top, right, bottom, uv;
        if (px + 14 <= 0 || y + 14 <= 0)
            continue;
        right = ((px + 14) * 4) & 4095;
        bottom = ((y + 14) * 4) & 4095;
        uv = (px < 0 ? (-px * 32) << 16 : 0) | (y < 0 ? -y * 32 : 0);
        left = px < 0 ? 0 : (px * 4) & 4095;
        top = y < 0 ? 0 : (y * 4) & 4095;
        lc_load_tab(palette, data + 528 + icon * 224, 16, 14);
        g = lc_gfx;
        g[0] = 0xFA000000;
        g[1] = color | (alpha & 255);
        g[2] = 0xE4000000 | (right << 12) | bottom;
        g[3] = (left << 12) | top;
        g[4] = 0xE1000000;
        g[5] = uv;
        g[6] = 0xF1000000;
        g[7] = 0x04000400;
        g[8] = 0xE7000000;
        g[9] = 0;
        lc_gfx = g + 10;
    }
}
