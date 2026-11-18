#ifndef OB64_GAME_CHAPTER_CARD_H
#define OB64_GAME_CHAPTER_CARD_H

#include "common/types.h"
#include "game/combat_draw_commands.h"

/* Shared chapter-card state used by setup, fade and reveal. */
typedef struct TitleImageHeader {
    u8 pad00[3];
    u8 field03;
    u16 width;
    u16 height;
    u8 pixels[0];
} TitleImageHeader;

typedef struct ChapterCard {
    TitleImageHeader *secondary[5];
    TitleImageHeader *images[5];
    s16 x[5];
    s16 y[5];
    u16 fadeStep[5];
    u16 alpha[5];
    /* Slot 1 tracks the fade. Slots 2 through 4 track the reveal. */
    u16 revealTicks[5];
    u8 revealSlot;
    u8 secondaryCount;
} ChapterCard;

extern ChapterCard *D_8023DE20;

extern TitleImageHeader *D_8023DE10;
extern TitleImageHeader *D_8023DE14;
extern TitleImageHeader *D_8023E594;
extern TitleImageHeader *D_8023E598;
extern TitleImageHeader *D_8023E59C;

extern TitleImageHeader *D_8023E580;
/* Setup replaces the loaded image header with its duplicated payload. */
extern void *D_8023E584;
extern TitleImageHeader *D_8023E588;
extern s32 D_8023E58C;
extern s32 D_8023E590;

extern CombatDrawCommand D_8022AA40[];
extern CombatDrawCommand *volatile gTitleCardDisplayListCursor;

void func_002893E4(void);
void func_00289558(void);
void func_00289504(void);

void func_002878F4(void);
void func_002879F8(void);
void func_00287A4C(void);
void func_00287AA4(void);

void func_00289780(void);
void func_00289D24(void);
s32 func_00289E88(void);
void func_00289ECC(void);
s32 func_0028ac88(s32 format, u32 width);
u32 func_0028acdc(s32 format, u32 width);

void func_0028A310(void);
void func_0028A678(void);
void func_0028A534(void);
void func_0028a6e4(void);

typedef void (*TitleSceneCallback)(s32 slot);

void func_00281F10(s32 slot);
void func_00281FA8(s32 slot);
void func_00282004(s32 slot);
void func_00282094(s32 slot);

#endif
