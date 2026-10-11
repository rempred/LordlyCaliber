#include "game/combat_pose_pool.h"

void func_00207A70(void)
{
    int i;
    int slot;

    if (D_801D0728 != 0) {
        for (slot = 0; slot < 20; slot++) {
            for (i = 0; i < D_801D0728[slot].field_18; i++) {
                if (D_801D0728[slot].field_38[i] != 0) {
                    func_000016C4(D_801D0728[slot].field_38[i]);
                    D_801D0728[slot].field_38[i] = 0;
                }
                if (D_801D0728[slot].field_3C[i] != 0) {
                    func_000016C4(D_801D0728[slot].field_3C[i]);
                    D_801D0728[slot].field_3C[i] = 0;
                }
                if (D_801D0728[slot].field_40[i] != 0) {
                    func_000016C4(D_801D0728[slot].field_40[i]);
                    D_801D0728[slot].field_40[i] = 0;
                }
            }
            for (i = 0; i < 10; i++) {
                if (D_801D0728[slot].field_70[i] != 0) {
                    func_000016C4(D_801D0728[slot].field_70[i]);
                    D_801D0728[slot].field_70[i] = 0;
                }
            }
        }
    }
}
