#include "game/chapter_card.h"

extern void func_80080998(void *destination, void *source, s32 size);
extern s32 func_0002CBCC(void);

static inline u8 revealActive(void)
{
    s32 i = 2;
    ChapterCard *card = D_8023DE20;

    do {
        if (card->revealTicks[(u8)i] != 0) {
            return 1;
        }
        ++i;
    } while ((u8)i < 5);
    return 0;
}

static inline s32 nonnegative(s32 value)
{
    return value & (~value >> 31);
}

void func_00289ECC(void)
{
    s32 i = 2;
    s16 row;
    s16 band;
    TitleImageHeader *image;

    do {
        u8 slot = i;
        ChapterCard *card = D_8023DE20;
        u16 ticks = card->revealTicks[slot];

        if (ticks != 0) {
            u16 left = ticks - 1;
            card->revealTicks[slot] = left;
            if (left != 0) {
                card->revealTicks[slot] = ticks - 2;
            }
            {
                ChapterCard *updated = D_8023DE20;
                if (updated->revealTicks[slot] == 2) {
                    s32 stride;
                    image = updated->secondary[slot];
                    stride = func_0028ac88(image->field03, image->width);

                    func_80080998(image, D_8023DE20->images[slot], stride * image->height + 8);
                    return;
                }
            }
            break;
        }
        ++i;
    } while ((u8)i < 5);

    if (revealActive()) {
        u8 slot = i;
        ChapterCard *card = D_8023DE20;
        image = card->secondary[slot];

        if (image != 0) {
            TitleImageHeader *mask = card->images[0];
            u16 width = image->width;
            s32 maskWidth = mask->width;

            s32 initialBand;
            s32 limited;
            u16 currentBand;
            s16 maskSkip;
            s16 imageSkip;
            s32 stride;
            s32 height;
            ChapterCard *buffers;
            u8 *dst;
            u8 *src;
            u8 *maskPixels;
            u8 *end;
            u16 strideWidth;

            /* The slot index is captured above. Reuse its word temporary for the countdown. */
            i = card->revealTicks[slot];
            initialBand = width + maskWidth - i;
            band = initialBand;
            if ((s16)initialBand > maskWidth) {
                band = maskWidth;
            }
            currentBand = band;
            limited = (s16)currentBand > i;
            maskSkip = maskWidth - currentBand;
            if (limited) {
                band = i;
            }
            /* Keep the unmodified halfword width for the stride call. */
            strideWidth = width;
            width -= i;
            imageSkip = (s16)width < 0 ? 0 : width;
            stride = func_0028ac88(image->field03, strideWidth);
            height = image->height;
            buffers = D_8023DE20;
            dst = image->pixels;
            maskPixels = buffers->images[0]->pixels;
            src = buffers->images[slot]->pixels;
            row = 0;
            end = (u8 *)image + stride * height + 8;

            if (height != 0) {
                u16 widthHalfword = band;
                s32 maskAdvance = maskSkip;
                s32 imageAdvance = imageSkip;
                s32 spanWidth = (s16)widthHalfword;
                do {
                    s16 pixel;

                    maskPixels += maskAdvance;
                    dst += imageAdvance;
                    src += imageAdvance;
                    if (dst < end) {
                        *dst = *src;
                    }
                    --dst;
                    --src;
                    if (dst < end) {
                        *dst = *src;
                    }
                    ++dst;
                    ++src;

                    /* Compare directly against the halfword band to retain the retail loop setup. */
                    for (pixel = 0; pixel < band; ++pixel) {
                        s32 coverage = *maskPixels;
                        s32 chance = func_0002CBCC() % 90;

                        chance = (chance * chance) % 90;
                        coverage = (coverage * chance) / 50;
                        coverage = nonnegative(coverage);
                        if (coverage >= 256) {
                            coverage = 255;
                        }
                        coverage *= *src;
                        coverage /= 255;
                        if (*dst < (u8)coverage) {
                            if (dst < end) {
                                *dst = coverage;
                            }
                        }
                        ++dst;
                        ++maskPixels;
                        ++src;
                    }
                    maskPixels += mask->width - maskAdvance - spanWidth;
                    dst += func_0028ac88(image->field03, image->width) - imageAdvance - spanWidth;
                    src += func_0028ac88(image->field03, image->width) - imageAdvance - spanWidth;
                    ++row;
                } while (row < image->height);
            }
        }
    }
}
