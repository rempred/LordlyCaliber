#include "game/chapter_card.h"

extern u16 D_8018F1A0;
extern void *D_8022A974;
extern s32 D_8022A990;
extern s16 D_8022A994;

extern void *func_00070F30(s32 size);
extern void bzero(void *buffer, s32 size);
extern void func_00283FA8(s32 scenario, s32 mode);
extern s32 func_00284288(void);
extern void func_002878F4(void);
extern void func_002893E4(void);
extern void *func_0002dfb8(void *buffer, u32 key);
extern void *func_0002e138(u32 key);
extern void resource_free(void *buffer);

void func_0028A310(void)
{
    u16 variant = (D_8018F1A0 & 0xFFF) - 1073;
    s32 secondaryCount = 0;
    s32 firstSecondary;
    u8 i;
    u8 child;
    u32 *keys;
    ChapterCard *card;
    TitleImageHeader *image;

    D_8022A974 = func_00070F30(0x1CB8);
    bzero(D_8022A974, 0x1CB8);
    func_00283FA8(D_8022A994, 0);
    firstSecondary = 0;
    D_8022A990 = 0;
    func_00284288();
    D_8023DE20 = func_00070F30(0x5C);
    bzero(D_8023DE20, 0x5C);

    switch (variant) {
    case 0:
        firstSecondary = 1;
        secondaryCount = 3;
        break;
    case 1:
        firstSecondary = 4;
        secondaryCount = 3;
        break;
    case 2:
        firstSecondary = 7;
        secondaryCount = 3;
        break;
    case 3:
        firstSecondary = 10;
        secondaryCount = 3;
        break;
    case 4:
        firstSecondary = 13;
        secondaryCount = 4;
        break;
    }

    /* Mission entry is a separate comparison after the five chapter cases. */
    if (variant == 5) {
        func_002878F4();
        func_002893E4();
        return;
    }

    D_8023DE20->secondaryCount = secondaryCount;
    keys = func_0002dfb8(0, 0x01890C90);
    child = firstSecondary;
    D_8023DE20->images[0] = func_0002e138(keys[0]);
    for (i = 1; i < secondaryCount + 1; ++i) {
        D_8023DE20->images[i] = func_0002e138(keys[child]);
        D_8023DE20->secondary[i] = func_0002e138(keys[child]);
        ++child;
    }
    resource_free(keys);
    card = D_8023DE20;
    image = card->images[1];
    card->x[1] = 160 - (image->width >> 1);
    card->y[1] = 120 - (image->height >> 1) - 16 * card->secondaryCount;
}
