typedef unsigned char u8;
typedef unsigned int u32;
extern u8 D_800AA050[][2];
void encode_ascii_to_glyph(u8 *jis, u8 *n64, u32 len) {
    u32 cnt;
    u32 cnt2;
    for (cnt = 0; cnt < len; cnt++) {
        if (*jis == 0) {
            break;
        }
        if ((*jis >= 'A') && (*jis <= 'Z')) {
            *n64 = *jis - 0x27;
        } else if (((u8)(*jis - 'a')) < 26) {
            *n64 = *jis - 0x47;
        } else if ((*jis >= '0') && (*jis <= '9')) {
            *n64 = *jis - 0x20;
        } else if ((*jis >= 0xB1) && (*jis <= 0xDC)) {
            if (*(jis + 1) == 0xDE) {
                if (*jis < 0xCA) {
                    *n64 = *jis - 0x3A;
                } else {
                    *n64 = *jis - 0x3F;
                }
                jis++;
            } else if (*(jis + 1) == 0xDF) {
                *n64 = *jis - 0x3A;
                jis++;
            } else {
                *n64 = *jis - 0x61;
            }
        } else {
            for (cnt2 = 0; D_800AA050[cnt2][1]; cnt2++) {
                if (D_800AA050[cnt2][0] == *jis) {
                    *n64 = D_800AA050[cnt2][1];
                    break;
                }
            }
            if (!D_800AA050[cnt2][1]) {
                *n64 = 0xF;
            }
        }
        n64++;
        jis++;
    }
}
