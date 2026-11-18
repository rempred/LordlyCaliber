#include "game/chapter_card.h"

extern u16 D_8018F1A0;
extern void func_00287A4C(void);

void func_0028A678(void)
{
    /* KMC must keep the second countdown load inside the active fade. */
    volatile ChapterCard *card = D_8023DE20;

    if (card->revealTicks[1] != 0) {
        u16 alpha = card->alpha[1];
        u16 ticks = card->revealTicks[1] - 1;
        u16 step = card->fadeStep[1];

        card->alpha[1] = alpha + step;
        card->revealTicks[1] = ticks;
    }

    func_00289ECC();
    if (D_8018F1A0 - 1073 == 5) {
        func_00287A4C();
    }
}
