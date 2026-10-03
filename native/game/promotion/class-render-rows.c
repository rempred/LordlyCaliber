/* Army class-row preparation. The surrounding renderer retains ownership of
 * drawing, scrolling and display lists. Coordinates keep their original list
 * index so the existing selected-class camera/cursor follows the same grid.
 */
#include "class_menu_config.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
extern u8 *volatile lc_army_owner;
/* These are float bit patterns. The first integer argument makes the original
 * matrix routine receive its three float arguments in a1/a2/a3 under o32.
 */
void lc_class_coordinates(u32 *, u32 *, int);
void lc_class_matrix(void *, u32, u32, u32);

void lc_class_render_rows(u8 *matrices) {
    u8 *owner = lc_army_owner;
    u32 flags = *(u16 *)(owner + 0x130) & 0x300;
    u32 x, y;
    while (flags) {
        u32 index = 0;
        u32 count;
#if LC_CLASS_COUNT > 81
        /* Both modes render the same shared list. Expanded families can exceed
         * the old class-change mode's 21 rows; draw the bounded window once.
         */
        count = *(u16 *)(owner + 0x1BFC);
        flags = 0;
#else
        if (flags & 0x100) {
            count = 21;
            flags &= ~0x100U;
        } else {
            count = *(u16 *)(owner + 0x1BFC);
            flags = 0;
        }
#endif
        if (count > LC_CLASS_COUNT)
            break;
#if LC_CLASS_COUNT > 81
        /* At most the retail full-list budget. Keep complete four-row columns
         * before the selected column, without relocating the visible coordinates.
         */
        if (count > 81) {
            u32 selected = *(u16 *)(owner + 0x5EA);
            if (selected > 40)
                index = (selected - 40) & 0xFFFCU;
            if (count > index + 81)
                count = index + 81;
        }
#endif
        for (; index < count; ++index) {
            u32 rowIndex = *(u32 *)(owner + 0x5C8);
            u16 entry = ((u16 *)(owner + LC_LIST_OFFSET))[index];
            u8 *row;
            if (rowIndex >= LC_DRAW_ROW_CAPACITY)
                break;
            row = owner + 0x32C + rowIndex * 4;
            if (!entry) {
                *(u16 *)(row + 2) = 0;
                break;
            }
#if LC_DISABLED_FLAG == 128
            row[0] = entry;
#else
            *(u16 *)row = entry;
#endif
            *(u16 *)(row + 2) = 1;
            lc_class_coordinates(&x, &y, index);
            lc_class_matrix(matrices + 0x1B10 + rowIndex * 64, x, y, 0);
            *(u32 *)(owner + 0x5C8) = rowIndex + 1;
        }
    }
}
