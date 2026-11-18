#include "game/chapter_card.h"

#define TILE_WIDTH(image) func_0028acdc((image)->field03, (image)->width)
#define TEXEL_COORD(value) (((u32)(value) << 2) & 0xFFF)
#define TILE_LINE(width) ((((u32)((width) + 7) >> 3) & 0x1FF) << 9)

static inline u32 screenEdge(s32 coordinate)
{
    s32 fixed = coordinate * 4;
    return (fixed & (~(s32)(s16)fixed >> 31)) & 0xFFC;
}

void func_00289780(void)
{
    u32 textureOn = 0xD7000002;
    u32 textureScale = 0x80008000;
    CombatDrawCommand *setup = gTitleCardDisplayListCursor;
    u32 commandWord = 0xE3000C00;
    ChapterCard *initial = D_8023DE20;
    u8 slot = 1;
    u8 alpha;

    gTitleCardDisplayListCursor = setup + 1;
    gTitleCardDisplayListCursor = setup + 2;
    setup[0].first = 0xE7000000;
    setup[0].second = 0;
    setup[1].first = 0xFA000000;
    alpha = ((u8 *)&initial->alpha[1])[1];
    gTitleCardDisplayListCursor = setup + 3;
    gTitleCardDisplayListCursor = setup + 4;
    gTitleCardDisplayListCursor = setup + 5;
    setup[2].first = textureOn;
    setup[2].second = textureScale;
    setup[3].first = 0xE7000000;
    setup[3].second = 0;
    setup[4].first = commandWord;
    setup[4].second = 0;
    setup[1].second = 0xFFFFFF00 | alpha;

    do {
        u8 index = slot;
        volatile ChapterCard *card = D_8023DE20;
        TitleImageHeader *image = card->images[index];
        TitleImageHeader *pixels = card->secondary[index];
        s16 rawX = card->x[index];
        u16 x = rawX;
        u16 y = card->y[index];

        if (image != 0) {
            u32 zeroPosition = (rawX == 0) & (y == 0);
            if (!zeroPosition) {
                CombatDrawCommand *mode = gTitleCardDisplayListCursor;
                u16 height;
                s32 row;
                u32 hasRows;

                mode[0].first = 0xE7000000;
                mode[0].second = 0;
                mode[1].first = 0xE200001C;
                mode[1].second = 0x00504240;
                mode[2].first = 0xFCFF97FF;
                mode[2].second = 0xFF2DFEFF;
                height = image->height;
                gTitleCardDisplayListCursor = mode + 1;
                gTitleCardDisplayListCursor = mode + 2;
                gTitleCardDisplayListCursor = mode + 3;
                do { hasRows = zeroPosition < height; } while (0);
                row = 0;
                if (hasRows) {
                    s32 fixedX;
                    s32 signedX;
                    s32 scaledX;

                    fixedX = x;
                    fixedX <<= 2;
                    signedX = (s16)(x * 4);
                    scaledX = signedX * 1024;
                    do {
                        s32 currentRow = (u16)row;
                        s32 remaining = height - currentRow;
                        s32 rows = 4;
                        s32 topTexel;
                        s32 bottomTexel;
                        s32 lastRow;
                        u32 tileWidth;
                        u16 imageWidth;
                        s32 top;
                        s32 sOrigin;
                        s32 tOrigin;
                        s32 yFixed;
                        s32 baseT;
                        CombatDrawCommand *imageCmd;
                        CombatDrawCommand *load;
                        CombatDrawCommand *renderTile;
                        CombatDrawCommand *size;
                        CombatDrawCommand *rect;
                        CombatDrawCommand *end;

                        if (remaining < 4) {
                            rows = remaining;
                        }
                        imageCmd = gTitleCardDisplayListCursor;
                        imageWidth = image->width;
                        gTitleCardDisplayListCursor = imageCmd + 1;
                        // Stage the texel coordinate separately from each tile width call.
                        do {
                            topTexel = currentRow * 4;
                        } while (0);
                        imageCmd->first = 0xFD880000 | ((func_0028acdc(image->field03, imageWidth) - 1) & 0xFFF);
                        imageCmd->second = (u32)((u8 *)pixels + 8);

                        imageCmd = gTitleCardDisplayListCursor;
                        imageWidth = image->width;
                        gTitleCardDisplayListCursor = imageCmd + 1;
                        do {
                            topTexel &= 0xFFF;
                        } while (0);
                        imageCmd->first = 0xF5880000 | TILE_LINE(func_0028acdc(image->field03, imageWidth));
                        imageCmd->second = 0x07000000;

                        load = gTitleCardDisplayListCursor;
                        gTitleCardDisplayListCursor = load + 1;
                        load[0].first = 0xE6000000;
                        gTitleCardDisplayListCursor = load + 2;
                        load[0].second = 0;
                        load[1].first = 0xF4000000 | topTexel;
                        tileWidth = TILE_WIDTH(image);
                        do {
                            lastRow = currentRow + rows;
                            tileWidth = TEXEL_COORD(tileWidth - 1) << 12;
                            bottomTexel = TEXEL_COORD(lastRow - 1);
                        } while (0);
                        renderTile = gTitleCardDisplayListCursor;
                        {
                            u32 tileEnd = bottomTexel | 0x07000000;
                            load[1].second = tileWidth | tileEnd;
                        }

                        gTitleCardDisplayListCursor = renderTile + 1;
                        renderTile[0].first = 0xE7000000;
                        renderTile[0].second = 0;
                        gTitleCardDisplayListCursor = renderTile + 2;
                        renderTile[1].first = 0xF5880000 | TILE_LINE(TILE_WIDTH(image));
                        renderTile[1].second = 0;

                        size = gTitleCardDisplayListCursor;
                        gTitleCardDisplayListCursor = size + 1;
                        size->first = 0xF2000000 | topTexel;
                        {
                            s32 right, left, leftMask, topField;
                            u32 sizeWidth;
                            s32 bottom;
                            u32 stepCommand;
                            u32 cursorWord;

                            do {
                                sizeWidth = TEXEL_COORD(TILE_WIDTH(image) - 1);
                            } while (0);

                            rect = gTitleCardDisplayListCursor;

                            sizeWidth <<= 12;
                            sizeWidth |= bottomTexel;

                            do {
                                top = y + row;
                                topField = (u32)top << 2;
                            } while (0);

                            size->second = sizeWidth;

                            rect[0].first = 0xE7000000;
                            rect[0].second = 0;
                            // Keep both rectangle words in this scope for KMC scheduling.
                            do {
                                right = image->width;
                                cursorWord = (u32)(rect + 1);
                                // Reuse the setup word so KMC schedules this address with the first advance.
                                commandWord = (u32)(rect + 2);
                                gTitleCardDisplayListCursor = (CombatDrawCommand *)cursorWord;
                                leftMask = ~signedX;
                                cursorWord = (u32)top << 18;
                                cursorWord = (s32)cursorWord >> 16;
                                cursorWord = ~cursorWord;
                                cursorWord = (s32)cursorWord >> 31;
                                topField &= cursorWord;
                                topField &= 0xFFC;
                                gTitleCardDisplayListCursor = (CombatDrawCommand *)commandWord;
                                gTitleCardDisplayListCursor = rect + 3;
                                stepCommand = 0xE1000000;

                                bottom = top + rows;
                                leftMask >>= 31;
                                left = fixedX & leftMask;
                                rect[2].first = stepCommand;
                                // Pack into the clipped X value to keep its reload ahead of the origin store.
                                left &= 0xFFC;
                                left <<= 12;
                                left |= topField;
                                rect[1].second = left;
                                rect[1].first = (screenEdge(x + right) << 12)
                                    | ((screenEdge(bottom) | 0xE4000000) & 0xFF000FFFU);
                            } while (0);
                        }

                        baseT = currentRow * 32;
                        if (signedX < 0) {
                            s32 clippedS = scaledX;
                            s32 mask;
                            clippedS >>= 7;
                            mask = clippedS < 1;
                            mask = -mask;
                            clippedS &= mask;
                            clippedS = -clippedS;
                            sOrigin = (u32)clippedS << 16;
                        } else {
                            sOrigin = 0;
                        }
                        yFixed = (s16)y;
                        yFixed += (u16)row;
                        yFixed = (u32)yFixed << 2;
                        if (yFixed < 0) {
                            s32 delta = (s32)((u32)(y + row) << 18) >> 13;
                            delta &= -(delta < 1);
                            tOrigin = (u16)(baseT - delta);
                            ((CombatDrawCommand *)commandWord)->second = sOrigin | tOrigin;
                        } else {
                            tOrigin = (u16)baseT;
                            ((CombatDrawCommand *)commandWord)->second = sOrigin | tOrigin;
                        }

                        end = gTitleCardDisplayListCursor;
                        row += 4;
                        end[0].first = 0xF1000000;
                        end[0].second = 0x04000400;
                        end[1].first = 0xE7000000;
                        end[1].second = 0;
                        height = image->height;
                        gTitleCardDisplayListCursor = end + 1;
                        gTitleCardDisplayListCursor = end + 2;
                    } while ((u16)row < height);
                }
            }
        }
        ++slot;
    } while (slot < 5);

    {
        CombatDrawCommand *sync = gTitleCardDisplayListCursor;
        gTitleCardDisplayListCursor = sync + 1;
        sync->first = 0xE7000000;
        sync->second = 0;
    }
}
