#include "game/chapter_card.h"
#include "game/cutscene_panels.h"

extern const u8 data_002867A0[];
extern u8 D_800E7A32;

extern u8 func_00042c30(void);
extern u8 func_002836BC(void);
extern void func_00290D50(void);

#define SCENE_PIPE_SYNC() { \
    CombatDrawCommand *sync = gTitleCardDisplayListCursor++; \
    sync->first = 0xE7000000; \
    sync->second = 0; \
}

#define SCENE_CLIP_END(command, lowerRight) { \
    gTitleCardDisplayListCursor = (command) + 1; \
    (command)[0].first = 0xE7000000; \
    (command)[0].second = 0; \
    gTitleCardDisplayListCursor = (command) + 2; \
    (command)[1].first = 0xED000000; \
    (command)[1].second = (lowerRight); \
    gTitleCardDisplayListCursor = (command) + 3; \
    (command)[2].first = 0xE7000000; \
    (command)[2].second = 0; \
}

void func_00281CE4(s32 slot)
{
    /* Use the ordinary cursor for the single viewport advance and the
     * clip reads. The clip writes retain the volatile cursor advances.
     */
    if (func_00042c30() == 0) {
        CombatDrawCommand *viewport = D_800E9BA0;
        D_800E9BA0 = viewport + 1;
        viewport->first = 0xDC080008;
        viewport->second = (u32)data_002867A0;
    } else {
        CombatDrawCommand *viewport = D_800E9BA0;
        D_800E9BA0 = viewport + 1;
        viewport->first = 0xDC080008;
        viewport->second = (u32)(data_002867A0 + 0x10);
    }

    if (func_00042c30() == 0) {
        CombatDrawCommand *command = D_800E9BA0;

        SCENE_CLIP_END(command, 0x004FC3BC);
    } else {
        CombatDrawCommand *command = D_800E9BA0;

        SCENE_CLIP_END(command, 0x00A00780);
    }

    SCENE_PIPE_SYNC();
    func_0028af64();
    if ((u8)(func_002836BC() ^ 0xD7) == 0) {
        u8 *flags = &D_800E7A32;

        *flags |= 2;
    }
    SCENE_PIPE_SYNC();

    if (func_00042c30() == 0) {
        CombatDrawCommand *command = D_800E9BA0;

        SCENE_CLIP_END(command, 0x004FC3BC);
    } else {
        CombatDrawCommand *command = D_800E9BA0;

        SCENE_CLIP_END(command, 0x00A00780);
    }

    SCENE_PIPE_SYNC();
    func_00290D50();
}
