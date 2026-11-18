#include "game/chapter_card.h"

extern void func_002908CC(TitleImageHeader *image);
extern void func_00287FB8(const u32 *pixels, s32 type, s32 format, u32 width,
                         u32 height, u32 tileWidth, u32 tileHeight,
                         float x0, float y0, float x1, float y1);
extern void func_0028752C(TitleImageHeader *image, s32 startX, s32 startY,
                         s32 width, s32 height, s32 x, s32 y);

#define COMPANION_SYNC() { \
    CombatDrawCommand *sync = gTitleCardDisplayListCursor++; \
    sync->first = 0xE7000000; \
    sync->second = 0; \
}

void func_00287AA4(void)
{
    TitleImageHeader *firstImage = D_8023E580;
    const u32 *pixels = D_8023E584;
    TitleImageHeader *lastImage;
    CombatDrawCommand *command;
    u16 width;
    u16 height;

    func_002908CC(firstImage);
    /* Read the cursor through its ordinary declaration so KMC can move the
     * pipe-sync constant into the preceding call's delay slot. The volatile
     * alias preserves each advance of the display-list cursor.
     */
    command = D_800E9BA0;
    gTitleCardDisplayListCursor = command + 1;
    command[0].first = 0xDE000000;
    command[0].second = (u32)D_8022AA40;
    gTitleCardDisplayListCursor = command + 2;
    command[1].first = 0xE3000C00;
    command[1].second = 0;
    gTitleCardDisplayListCursor = command + 3;
    command[2].first = 0xE7000000;
    command[2].second = 0;
    gTitleCardDisplayListCursor = command + 4;
    command[3].first = 0xE200001C;
    command[3].second = 0x00504240;
    gTitleCardDisplayListCursor = command + 5;
    command[4].first = 0xFC119623;
    command[4].second = 0xFF2FFFFF;
    gTitleCardDisplayListCursor = command + 6;
    command[5].first = 0xFA000000;
    command[5].second = 0xFFFFFF60;
    gTitleCardDisplayListCursor = command + 7;
    command[6].first = 0xE7000000;
    command[6].second = 0;
    gTitleCardDisplayListCursor = command + 8;
    command[7].first = 0xE200001C;
    command[7].second = 0x00504240;
    gTitleCardDisplayListCursor = command + 9;
    command[8].first = 0xFC119623;
    command[8].second = 0xFF2FFFFF;
    gTitleCardDisplayListCursor = command + 10;
    command[9].first = 0xFA000000;
    command[9].second = 0xFFFFFF60;
    gTitleCardDisplayListCursor = command + 11;
    command[10].first = 0xE7000000;
    command[10].second = 0;

    /* Each counter selects the pixels for a full set of four rectangles. */
    func_00287FB8(pixels + D_8023E58C, 0, 3, 160, 120, 40, 20,
                  0.0f, 0.0f, 160.0f, 120.0f);
    COMPANION_SYNC();
    func_00287FB8(pixels + D_8023E58C, 0, 3, 160, 120, 40, 20,
                  160.0f, 0.0f, 320.0f, 120.0f);
    COMPANION_SYNC();
    func_00287FB8(pixels + D_8023E58C, 0, 3, 160, 120, 40, 20,
                  0.0f, 120.0f, 160.0f, 240.0f);
    COMPANION_SYNC();
    func_00287FB8(pixels + D_8023E58C, 0, 3, 160, 120, 40, 20,
                  160.0f, 120.0f, 320.0f, 240.0f);
    COMPANION_SYNC();
    func_00287FB8(pixels + D_8023E590, 0, 3, 160, 120, 40, 20,
                  0.0f, 0.0f, 160.0f, 120.0f);
    COMPANION_SYNC();
    func_00287FB8(pixels + D_8023E590, 0, 3, 160, 120, 40, 20,
                  160.0f, 0.0f, 320.0f, 120.0f);
    COMPANION_SYNC();
    func_00287FB8(pixels + D_8023E590, 0, 3, 160, 120, 40, 20,
                  0.0f, 120.0f, 160.0f, 240.0f);
    COMPANION_SYNC();
    func_00287FB8(pixels + D_8023E590, 0, 3, 160, 120, 40, 20,
                  160.0f, 120.0f, 320.0f, 240.0f);

    {
        CombatDrawCommand *lastCommand = gTitleCardDisplayListCursor;

        lastImage = D_8023E588;
        lastCommand[0].first = 0xE7000000;
        lastCommand[0].second = 0;
        lastCommand[1].first = 0xFCFFFFFF;
        lastCommand[1].second = 0xFFFEF379;
        lastCommand[2].first = 0xFB000000;
        lastCommand[2].second = 0;
        lastCommand[3].first = 0xE7000000;
        lastCommand[3].second = 0;

        width = lastImage->width;
        height = lastImage->height;
        gTitleCardDisplayListCursor = lastCommand + 1;
        gTitleCardDisplayListCursor = lastCommand + 2;
        gTitleCardDisplayListCursor = lastCommand + 3;
        gTitleCardDisplayListCursor = lastCommand + 4;
    }
    func_0028752C(lastImage, 0, 0, width, height, 0, 0);
    COMPANION_SYNC();
}
