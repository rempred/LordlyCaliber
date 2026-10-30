#include "common/types.h"

typedef struct RecordValue {
    u8 unknown00[4];
    s32 value04;
} RecordValue;

typedef struct ResultRecord {
    u16 state;
    u16 flags;
    RecordValue *value04;
    s32 value08;
    s16 code;
    u8 byte0E;
    u8 components[3];
    u8 byte12;
    u8 pad13;
} ResultRecord;

extern u8 D_80220DB8[];
extern ResultRecord *D_80220E08;
extern s16 D_80220078[][3];
extern s32 D_801CEAB0;
s32 func_001FFBFC(void);
s32 func_001FFCC8(void);
s32 func_001FFDA4(void);
void func_00260210(u16 *source);
void func_0020BD8C(s32 value);

void func_002631CC(void)
{
    u16 values[5];
    u16 restore[5];
    ResultRecord *record = (ResultRecord *)D_80220DB8;
    s32 slot = 0;
    s32 i;

    for (; slot < 4; slot++, record++) {
        if (record->state != 0) {
            switch (record->state) {
            case 1:
                if (record == D_80220E08) {
                    i = 0;
                    values[0] = 10;
                    for (; i < 3; i++) {
                        values[i + 1] = record->components[i] * D_80220078[D_801CEAB0][i] / 100;
                        if ((s16)values[i + 1] < 0) {
                            values[i + 1] = 0;
                        } else if ((s16)values[i + 1] >= 255) {
                            values[i + 1] = 255;
                        }
                    }
                    /* Retail does not write values[4] before this call. */
                    func_00260210(values);
                }
                /* Fall through to advance the state. */
            case 3:
            case 4:
                record->state++;
                break;
            case 2:
                if (record->value04->value04 != record->value08) {
                    record->state++;
                }
                break;
            case 5:
                if (record == D_80220E08) {
                    restore[0] = 10;
                    restore[1] = (u8)func_001FFBFC();
                    restore[2] = (u8)func_001FFCC8();
                    restore[3] = (u8)func_001FFDA4();
                    restore[4] = 255;
                    func_00260210(restore);
                }
                func_0020BD8C(record->byte0E);
                *(float *)0x801D0768 = (float)*(s32 *)0x801D0838;
                *(float *)0x801D0718 = (float)*(s32 *)0x801D0734;
                record->state = 0;
                *(s16 *)0x801D0820 = 0;
                *(float *)0x801D0814 = (float)*(s32 *)0x801D070C;
                *(s16 *)0x801D071C = 255;
                break;
            }
        }
    }
}
