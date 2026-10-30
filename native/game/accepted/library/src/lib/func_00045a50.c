/* This established symbol avoids the extra frame produced for a numeric base. */
extern unsigned char D_8018C410[][32];

int func_00045a50(unsigned short item0, unsigned short item1,
                  unsigned short item2, unsigned short item3)
{
    if (D_8018C410[item0][0] == 23) {
        return item0;
    }

    if (D_8018C410[item1][0] == 23) {
        return item1;
    }

    if (D_8018C410[item2][0] == 23) {
        return item2;
    }

    return D_8018C410[item3][0] == 23 ? item3 : 0;
}
