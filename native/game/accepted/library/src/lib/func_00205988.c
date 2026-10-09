#include "game/combat_pose_pool.h"

void func_00205988(int handle, int field0C)
{
    u32 i, j;

    if (D_801D0728 != 0) {
        for (i = 0; i < 20; i++) {
            if (D_801D0728[i].field_00 == handle &&
                D_801D0728[i].field_0C == field0C) {
                for (j = 0; j < D_801D0728[i].field_18; j++)
                    func_000016C4(D_801D0728[i].field_38[j]);
                for (j = 0; j < D_801D0728[i].field_18; j++)
                    func_000016C4(D_801D0728[i].field_3C[j]);
                for (j = 0; j < D_801D0728[i].field_18; j++)
                    func_000016C4(D_801D0728[i].field_40[j]);
                for (j = 0; j < 10; j++)
                    func_000016C4(D_801D0728[i].field_70[j]);
                func_000016C4(D_801D0728[i].field_08);
                func_000016C4(D_801D0728[i].field_48);
                func_000016C4(D_801D0728[i].field_44);
                func_000016C4(D_801D0728[i].field_6C);
                func_000016C4(D_801D0728[i].field_2C);
                func_00023780(&D_801D0728[i], 0xB0);
            }
        }
    }
}
