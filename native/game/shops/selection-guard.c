/* Normal C calling convention; the caller retains its full button word so
 * cancelling still works when a category is empty or the cursor is invalid. */
typedef struct { short first, field_02, field_04, last, field_08; } ShopRow;
extern ShopRow *D_80219F40;
unsigned int lc_shop_can_buy(unsigned int pressed, unsigned int category)
{
    const ShopRow *row;
    if (!pressed || category >= 7) return 0;
    row = &D_80219F40[category];
    return row->last >= 0 && row->first >= 0 && row->first <= row->last;
}
