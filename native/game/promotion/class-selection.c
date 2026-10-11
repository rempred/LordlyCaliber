/* Modified-game Army class selection, based on func_000C91A0's observed
 * four-row navigation and button precedence. The export supplies the layout.
 */
#include "class_menu_config.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
void lc_menu_sound(int);
void lc_menu_update(void);
void lc_menu_equipment(void);
extern u8 *volatile lc_army_owner;
extern volatile u16 lc_input_buttons;
extern volatile u16 lc_repeat_buttons;
#define OWNER lc_army_owner
#if LC_DISABLED_FLAG == 128
#define CLASS_MASK 127
#else
#define CLASS_MASK 255
#endif

int lc_class_selection(void)
{
    u8 *data = OWNER;
    u32 original = *(u16 *)(data + 0x5EA);
    u32 selected = original;
    u16 buttons = lc_input_buttons;
    u16 repeat;
    u32 count;
    int result = 0;
    if (buttons & 0x1000)
        result = 3;
    else if (buttons & 0x8000)
        result = 1;
    else if (buttons & 0x4000) {
        lc_menu_sound(1);
        result = 2;
    } else {
        repeat = lc_repeat_buttons;
        if (repeat & 0x800) {
            if (selected & 3)
                --selected;
        } else if (repeat & 0x400) {
            if ((selected & 3) != 3)
                ++selected;
        } else if (repeat & 0x200) {
            if (selected >= 4)
                selected -= 4;
        } else if (repeat & 0x100) {
            count = *(u16 *)(data + 0x1BFC);
            if (count && (selected >> 2) < ((count - 1) >> 2))
                selected += 4;
        }
        *(u16 *)(data + 0x5EA) = selected;
    }
    data = OWNER;
    selected = *(u16 *)(data + 0x5EA);
    if (original == selected)
        return result;
    count = *(u16 *)(data + 0x1BFC);
    if (!count) {
        *(u16 *)(data + 0x5EA) = 0;
        return result;
    }
    if (selected >= count) {
        selected = count - 1;
        *(u16 *)(data + 0x5EA) = selected;
    }
    *(u16 *)(data + 0x5E8) = ((u16 *)(data + LC_LIST_OFFSET))[selected] & CLASS_MASK;
    lc_menu_update();
    data = OWNER;
    if (*(u16 *)(data + 0x130) & 0x200)
        lc_menu_equipment();
    return result;
}
