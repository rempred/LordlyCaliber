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

extern CardGraphics *D_800E9BE0;
/* Ordinary access to the counter also exposed through the title-card volatile view. */
extern u16 gCardMatrixCount;
extern u16 D_800E79E0;
extern CardMatrix *D_801969B0;
extern CombatDrawVertex *D_80197178;

extern Panel *func_000552c0(void);
extern Panel *func_0005526c(u16);
extern void func_00056adc(void);
extern void func_00291854(Panel *);

void func_00299AC8(void)
{
    Panel *panel = func_000552c0();
    u8 slot;

    if (panel == 0) {
        return;
    }
    ((u8 *)D_8018FDC0)[0xD2] = 0;
    if (panel->field16 & 4) {
        panel = func_0005526c(0x100);
    }

    for (slot = 0; slot < 14; ++slot) {
        PanelSlot *record = D_8022A974->slots[slot];
        u32 contentOrKey;

        /* KMC reuses one register for the content pointer and its key. */
        if (record != 0 && (contentOrKey = (u32)panel->content) != 0 &&
            (contentOrKey = ((PanelContent *)contentOrKey)->key) == (u32)record->key) {
            CardGraphics *graphics = D_800E9BE0;
            u16 matrixCount = gCardMatrixCount;
            u16 vertexCount;
            u16 preset;

            gCardMatrixCount = matrixCount + (D_801969B0 - (graphics->matrices + matrixCount));
            vertexCount = D_800E79E0;
            D_800E79E0 = vertexCount + (D_80197178 - (graphics->vertices + vertexCount));

            preset = contentOrKey & 0xFF;
            func_00056adc();
            {
                CardGraphics *current = D_800E9BE0;
                D_801969B0 = current->matrices + gCardMatrixCount;
                D_80197178 = current->vertices + D_800E79E0;
            }

            if (((preset == 6) | (preset == 11)) || preset == 16) {
                s32 count = 0;
                Panel *node = D_8018FDC0->link04;
            nextPanel:
                if (node != 0) {
                    if (node == panel) {
                        func_00291854(node);
                    }
                    node = node->link04;
                    if (++count < 10) {
                        goto nextPanel;
                    }
                }
            }
            break;
        }
    }
    ((u8 *)D_8018FDC0)[0xD2] = 1;
}
