typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
extern s16 *D_801D069C;
s16 func_00201908(s32 a, s32 b, s16 *out) {
    s16 pad[1];
    s16 *row;
    s16 v;
    row = (s16 *)((u8 *)D_801D069C + D_801D069C[a]);
    row += b * 11;
    v = row[0];
    if (v != -1) {
        if (out == ((void *)0)) {
            return v;
        }
        if (v == -2) {
            out[0] = row[1];
            out[1] = row[2];
            out[2] = row[3];
            out[6] = row[4];
            out[7] = row[5];
            out[8] = row[6];
            out[9] = row[7];
            out[10] = row[8];
            out[11] = row[9];
            out[5] = row[10];
        } else {
            out[0] = v;
            out[1] = row[1];
            out[2] = row[2];
            out[3] = row[3];
            out[4] = row[4];
            out[5] = row[5];
            out[7] = 1;
        }
    }
    return row[0];
}
