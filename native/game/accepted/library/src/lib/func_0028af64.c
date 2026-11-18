#include "game/chapter_card.h"
#include "game/cutscene_panels.h"
#include "game/combat_draw.h"

typedef struct CardMatrix {
    u32 words[16];
} CardMatrix;

typedef struct CardGraphics {
    u8 pad00[8];
    CardMatrix *matrices;
    u8 pad0C[4];
    CombatDrawVertex *vertices;
} CardGraphics;

extern volatile u8 D_800A81F0;
extern CardGraphics *D_800E9BE0;
extern volatile u16 D_800C4BE4;
extern u16 D_800E79E0;
extern u8 D_8022A996;
extern CardMatrix *D_801969B0;
extern CombatDrawVertex *D_80197178;

extern u8 func_00042c30(void);
extern void func_0028B504(void);
extern void func_0028B7A4(void);
extern void func_002908CC(TitleImageHeader *image);
extern void func_00299AC8(void);

void func_0028af64(void)
{
    {
        /* The indexed pool expressions preserve the folded load registers. */
        volatile CardGraphics *graphics = D_800E9BE0;

        D_8022A996 = D_800A81F0;
        D_801969B0 = &graphics->matrices[D_800C4BE4];
        D_80197178 = &graphics->vertices[D_800E79E0];
    }

    if (func_00042c30() != 0) {
        func_0028B504();
        func_0028B7A4();
    } else {
        PanelParameters *parameters = D_8023DE34;

        if (parameters->field66 != 2 && parameters->field14 != 0) {
            func_002908CC(parameters->field14);
        }
    }
    func_00299AC8();

    {
        CardGraphics *graphics = D_800E9BE0;
        u16 matrixCount = D_800C4BE4;
        u16 vertexCount;

        D_800C4BE4 = matrixCount + (D_801969B0 - (graphics->matrices + matrixCount));
        vertexCount = D_800E79E0;
        D_800E79E0 = vertexCount + (D_80197178 - (graphics->vertices + vertexCount));
    }
}
