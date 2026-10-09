#include "game/combat_pose_pool.h"

void func_002057DC(void)
{
    u32 i, j;

    if (D_801D0728 != 0) {
        for (i = 0; i < 20; i++) {
            for (j = 0; j < D_801D0728[i].field_18; j++) {
                if (D_801D0728[i].field_38[j] != 0)
                    func_000016C4(D_801D0728[i].field_38[j]);
                if (D_801D0728[i].field_3C[j] != 0)
                    func_000016C4(D_801D0728[i].field_3C[j]);
                if (D_801D0728[i].field_40[j] != 0)
                    func_000016C4(D_801D0728[i].field_40[j]);
            }
            for (j = 0; j < 10; j++) {
                if (D_801D0728[i].field_70[j] != 0)
                    func_000016C4(D_801D0728[i].field_70[j]);
            }
            func_000016C4(D_801D0728[i].field_08);
            func_000016C4(D_801D0728[i].field_48);
            func_000016C4(D_801D0728[i].field_44);
            func_000016C4(D_801D0728[i].field_6C);
            func_000016C4(D_801D0728[i].field_2C);
        }
        func_000016C4(D_801D0728);
        D_801D0728 = 0;
    }
}
